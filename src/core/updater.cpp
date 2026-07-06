#include "core/updater.h"
#include "core/version.h"
#include "core/selfprotect.h"

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QCryptographicHash>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QProcess>
#include <QTimer>
#include <QCoreApplication>
#include <QUrl>

namespace {
// Canal de update do SPHONE, repo proprio LeonardoHGB/Soften-Phone (o SoftenPhone
// .NET antigo ficou em LeonardoHGB/SPhone). O release 'latest' deve conter os assets
// sphone-version.json e SPHONE-Setup-x.y.z.exe.
const QString kManifestUrl = QStringLiteral(
    "https://github.com/LeonardoHGB/Soften-Phone/releases/latest/download/sphone-version.json");
}

namespace sphone {

Updater::Updater(QObject* parent) : QObject(parent) {
    m_net = new QNetworkAccessManager(this);
}

QString Updater::currentVersion() { return QStringLiteral(SPHONE_VERSION); }

bool Updater::isTrustedUrl(const QString& url) {
    const QUrl u(url);
    if (u.scheme() != QLatin1String("https")) return false;
    const QString host = u.host().toLower();
    return host == "github.com" || host.endsWith(".github.com")
        || host.endsWith(".githubusercontent.com");
}

int Updater::compareVersions(const QString& a, const QString& b) {
    const QStringList pa = a.split('.'), pb = b.split('.');
    for (int i = 0; i < 3; ++i) {
        const int va = i < pa.size() ? pa[i].toInt() : 0;
        const int vb = i < pb.size() ? pb[i].toInt() : 0;
        if (va != vb) return va - vb;
    }
    return 0;
}

void Updater::checkForUpdate() {
    QNetworkRequest req{ QUrl(kManifestUrl) };
    req.setRawHeader("User-Agent", "SPHONE-Updater");
    QNetworkReply* reply = m_net->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) { emit checkFailed(reply->errorString()); return; }

        const QJsonObject o = QJsonDocument::fromJson(reply->readAll()).object();
        UpdateInfo info;
        info.version = o.value("version").toString();
        info.url = o.value("url").toString();
        info.sha256 = o.value("sha256").toString();
        info.notes = o.value("notes").toString();

        if (info.version.isEmpty() || info.url.isEmpty()
            || !isTrustedUrl(info.url) || info.sha256.isEmpty()) {
            emit checkFailed(QStringLiteral("Manifesto de atualizacao invalido.")); return;
        }
        if (compareVersions(info.version, currentVersion()) <= 0) { emit upToDate(); return; }
        emit updateAvailable(info);
    });
}

void Updater::downloadAndRun(const UpdateInfo& info) {
    // Nome FIXO (nao interpola info.version, que vem do JSON) em %TEMP%.
    const QString path = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
                             .filePath(QStringLiteral("SPHONE-Setup.exe"));

    // Grava em STREAMING para o disco (sem bufferizar ~44 MB na RAM) e alimenta o
    // hash em blocos, para nao travar a thread da UI no fim do download.
    auto* file = new QFile(path);
    if (!file->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        delete file;
        emit downloadFailed(QStringLiteral("Falha ao gravar o instalador.")); return;
    }
    auto* hash = new QCryptographicHash(QCryptographicHash::Sha256);

    QNetworkRequest req{ QUrl(info.url) };
    req.setRawHeader("User-Agent", "SPHONE-Updater");
    QNetworkReply* reply = m_net->get(req);

    connect(reply, &QNetworkReply::downloadProgress, this, [this](qint64 r, qint64 t) {
        if (t > 0) emit downloadProgress(int(r * 100 / t));
    });
    connect(reply, &QNetworkReply::readyRead, this, [reply, file, hash] {
        const QByteArray chunk = reply->readAll();
        if (!chunk.isEmpty()) { hash->addData(chunk); file->write(chunk); }
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, file, hash, info, path] {
        reply->deleteLater();
        auto cleanup = [file, hash] { file->close(); delete file; delete hash; };

        if (reply->error() != QNetworkReply::NoError) {
            const QString err = reply->errorString();
            file->close(); file->remove(); delete file; delete hash;
            emit downloadFailed(err); return;
        }
        // Ultimo bloco que possa ter chegado junto com o 'finished'.
        const QByteArray tail = reply->readAll();
        if (!tail.isEmpty()) { hash->addData(tail); file->write(tail); }
        file->flush();

        const QString actual = QString::fromLatin1(hash->result().toHex());
        const bool ok = actual.compare(info.sha256.trimmed(), Qt::CaseInsensitive) == 0;
        if (!ok) {
            file->close(); file->remove(); delete file; delete hash;
            emit downloadFailed(QStringLiteral("A verificacao de integridade (SHA256) falhou.")); return;
        }
        cleanup();
        launchInstallerAndQuit(path);
    });
}

void Updater::launchInstallerAndQuit(const QString& installerPath) {
    // Libera a auto-protecao ANTES de lancar o setup: se o encerramento limpo
    // demorar (teardown do PJSIP/audio), o instalador precisa poder fechar/forcar
    // o app para substituir o SPHONE.exe — senao o /VERYSILENT trava esperando.
    sphone::restoreProcessTermination();

    // Passa o PID para o instalador esperar este processo sair antes de trocar os
    // arquivos (setup.iss le {param:WaitPid}). Fim da corrida do delay fixo.
    const qint64 selfPid = QCoreApplication::applicationPid();
    qint64 pid = 0;
    const bool started = QProcess::startDetached(
        installerPath,
        { QStringLiteral("/VERYSILENT"), QStringLiteral("/SUPPRESSMSGBOXES"),
          QStringLiteral("/NORESTART"), QStringLiteral("/WaitPid=%1").arg(selfPid) },
        QString(), &pid);
    if (!started) {
        emit downloadFailed(QStringLiteral("Nao foi possivel iniciar o instalador.")); return;
    }
    // Encerra limpo; o setup aguarda este PID sumir antes de instalar/relancar.
    QTimer::singleShot(0, qApp, &QCoreApplication::quit);
}

}  // namespace sphone
