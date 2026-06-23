#include "data/exitaudit.h"

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QEventLoop>
#include <QTimer>
#include <QDateTime>
#include <QUrl>

// Webhook fora do versionamento: secret.h (gitignored) tem prioridade; sem ele,
// o modelo (secret.example.h) deixa o webhook vazio -> auditoria nao posta.
#if __has_include("data/secret.h")
#  include "data/secret.h"
#else
#  include "data/secret.example.h"
#endif

namespace {

const QString kExitWebhook = QStringLiteral(SPHONE_DISCORD_EXIT_WEBHOOK);

QString envOr(const char* name) {
    const QString v = qEnvironmentVariable(name);
    return v.isEmpty() ? QStringLiteral("?") : v;
}

}  // namespace

namespace sphone {

void ExitAudit::report(Event ev, const QString& ramal) {
    if (kExitWebhook.isEmpty()) return;   // sem webhook configurado: desligado

    QString title;
    int color;
    switch (ev) {
        case ExitAttemptDenied:
            title = QString::fromUtf8("🔒 Tentativa de encerramento negada");
            color = 0xE74C3C;  // vermelho
            break;
        case ExitBySupervisor:
            title = QString::fromUtf8("🛑 Soften Phone encerrado (supervisor)");
            color = 0xE67E22;  // laranja
            break;
        default:
            title = QString::fromUtf8("🛑 Soften Phone encerrado");
            color = 0x95A5A6;  // cinza
            break;
    }

    auto field = [](const QString& n, const QString& v) {
        return QJsonObject{ { "name", n },
                            { "value", v.trimmed().isEmpty() ? QStringLiteral("-") : v },
                            { "inline", true } };
    };

    QJsonArray fields;
    fields.append(field(QStringLiteral("Ramal"), ramal));
    fields.append(field(QString::fromUtf8("Máquina"), envOr("COMPUTERNAME")));
    fields.append(field(QString::fromUtf8("Usuário Windows"), envOr("USERNAME")));
    fields.append(field(QString::fromUtf8("Horário"),
                        QDateTime::currentDateTime().toString("dd/MM/yyyy HH:mm:ss")));

    const QJsonObject embed{ { "title", title }, { "color", color }, { "fields", fields } };
    const QJsonObject payload{ { "embeds", QJsonArray{ embed } } };

    QNetworkAccessManager net;
    QNetworkRequest req{ QUrl(kExitWebhook) };
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("User-Agent", "SPHONE-Audit");
    QNetworkReply* reply = net.post(req, QJsonDocument(payload).toJson(QJsonDocument::Compact));

    // Bloqueia ate o POST terminar (ou ate o timeout) para garantir o envio antes
    // de o processo sair. reply e filho do 'net' local -> destruido com ele.
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeout.start(3000);
    loop.exec();
}

}  // namespace sphone
