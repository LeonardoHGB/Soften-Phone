#include "core/updater.h"
#include "core/version.h"

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
// Canal de update do SPHONE. O release deve conter os assets sphone-version.json
// e SPHONE-Setup-x.y.z.exe. (Asset proprio p/ nao colidir com o version.json do
// SoftenPhone antigo no mesmo repo.)
const QString kManifestUrl = QStringLiteral(
    "https://github.com/LeonardoHGB/SPhone/releases/latest/download/sphone-version.json");
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
    QNetworkRequest req{ QUrl(info.url) };
    req.setRawHeader("User-Agent", "SPHONE-Updater");
    QNetworkReply* reply = m_net->get(req);
    connect(reply, &QNetworkReply::downloadProgress, this, [this](qint64 r, qint64 t) {
        if (t > 0) emit downloadProgress(int(r * 100 / t));
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, info] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) { emit downloadFailed(reply->errorString()); return; }

        const QByteArray data = reply->readAll();
        const QString actual = QString::fromLatin1(
            QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
        if (actual.compare(info.sha256.trimmed(), Qt::CaseInsensitive) != 0) {
            emit downloadFailed(QStringLiteral("A verificacao de integridade (SHA256) falhou.")); return;
        }

        // Nome FIXO (nao interpola info.version, que vem do JSON) em %TEMP%.
        const QString path = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
                                 .filePath(QStringLiteral("SPHONE-Setup.exe"));
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate) || f.write(data) != data.size()) {
            emit downloadFailed(QStringLiteral("Falha ao gravar o instalador.")); return;
        }
        f.close();

        // Roda o instalador em silencio; ele fecha o app, troca os arquivos e reabre.
        // So encerramos o app se o instalador REALMENTE iniciou (senao o usuario
        // ficaria sem app e sem update).
        qint64 pid = 0;
        const bool ok = QProcess::startDetached(
            path, { "/VERYSILENT", "/SUPPRESSMSGBOXES", "/NORESTART" }, QString(), &pid);
        if (!ok) { emit downloadFailed(QStringLiteral("Nao foi possivel iniciar o instalador.")); return; }
        QTimer::singleShot(300, qApp, &QCoreApplication::quit);   // libera o exe p/ o setup trocar
    });
}

}  // namespace sphone
