#include "data/discordaudit.h"

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonArray>
#include <QUrl>

// Webhook fora do versionamento: secret.h (gitignored) tem prioridade; sem ele,
// o modelo (secret.example.h) deixa o webhook vazio -> auditoria nao posta.
#if __has_include("data/secret.h")
#  include "data/secret.h"
#else
#  include "data/secret.example.h"
#endif

namespace {

// Webhook de auditoria do Discord (vem de secret.h).
const QString kWebhook = QStringLiteral(SPHONE_DISCORD_WEBHOOK);

constexpr int kBlue  = 0x3498DB;   // em andamento
constexpr int kGreen = 0x2ECC71;   // atendida
constexpr int kRed   = 0xE74C3C;   // perdida/recusada/falha
constexpr int kGray  = 0x95A5A6;   // atendida em outro ramal

QString formatDuration(int seconds) {
    if (seconds <= 0) return QString::fromUtf8("—");
    const int h = seconds / 3600, m = (seconds % 3600) / 60, s = seconds % 60;
    return h > 0 ? QStringLiteral("%1:%2:%3").arg(h, 2, 10, QChar('0')).arg(m, 2, 10, QChar('0')).arg(s, 2, 10, QChar('0'))
                 : QStringLiteral("%1:%2").arg(m, 2, 10, QChar('0')).arg(s, 2, 10, QChar('0'));
}

QByteArray body(const QJsonObject& payload) {
    return QJsonDocument(payload).toJson(QJsonDocument::Compact);
}

QNetworkRequest jsonReq(const QString& url) {
    QNetworkRequest req{ QUrl(url) };
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("User-Agent", "SPHONE-Audit");
    return req;
}

}  // namespace

namespace sphone {

DiscordAudit::DiscordAudit(QObject* parent) : QObject(parent) {
    m_net = new QNetworkAccessManager(this);
}

QJsonObject DiscordAudit::buildPayload(const CallAudit& c, bool inProgress) const {
    const bool inbound = c.direction == CallDirection::Inbound;
    const QString title = inProgress
        ? (inbound ? QString::fromUtf8("📞 Recebendo chamada…") : QString::fromUtf8("📲 Ligando…"))
        : (inbound ? QString::fromUtf8("📞 Chamada recebida")   : QString::fromUtf8("📲 Chamada efetuada"));
    const int color = inProgress ? kBlue
                    : c.answeredElsewhere ? kGray
                    : (c.answered ? kGreen : kRed);

    auto field = [](const QString& n, const QString& v) {
        return QJsonObject{ { "name", n }, { "value", v }, { "inline", true } };
    };

    QJsonArray fields;
    fields.append(field(QString::fromUtf8("Número"),
                        c.peerNumber.trimmed().isEmpty() ? QStringLiteral("desconhecido") : c.peerNumber));
    if (!c.peerName.trimmed().isEmpty())
        fields.append(field(QStringLiteral("Nome"), c.peerName));
    fields.append(field(QStringLiteral("Ramal"), c.ramal.trimmed().isEmpty() ? QStringLiteral("-") : c.ramal));
    fields.append(field(QString::fromUtf8("Início"), c.startedLocal.toString("dd/MM/yyyy HH:mm:ss")));
    if (!inProgress)
        fields.append(field(QString::fromUtf8("Duração"), formatDuration(c.durationSeconds)));
    fields.append(field(QStringLiteral("Resultado"), c.outcome));

    QJsonObject embed{ { "title", title }, { "color", color }, { "fields", fields } };
    return QJsonObject{ { "embeds", QJsonArray{ embed } } };
}

void DiscordAudit::postStart(const CallAudit& c) {
    if (kWebhook.isEmpty()) return;   // sem webhook configurado: auditoria desligada
    m_pendingId.clear();
    m_endPending = false;
    m_endPayload = QJsonObject();
    m_startInFlight = true;

    // ?wait=true faz o Discord responder com o objeto da mensagem (inclui o id).
    QNetworkReply* reply = m_net->post(jsonReq(kWebhook + "?wait=true"), body(buildPayload(c, true)));
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        m_startInFlight = false;
        if (reply->error() == QNetworkReply::NoError) {
            const QJsonObject o = QJsonDocument::fromJson(reply->readAll()).object();
            m_pendingId = o.value("id").toString();
        }
        if (m_endPending) { m_endPending = false; sendEnd(m_endPayload); }
    });
}

void DiscordAudit::postEnd(const CallAudit& c) {
    if (kWebhook.isEmpty()) return;
    const QJsonObject payload = buildPayload(c, false);
    if (m_startInFlight) {            // id ainda nao chegou -> envia quando o POST inicial terminar
        m_endPending = true;
        m_endPayload = payload;
        return;
    }
    sendEnd(payload);
}

void DiscordAudit::sendEnd(const QJsonObject& payload) {
    if (!m_pendingId.isEmpty()) {
        QNetworkReply* reply = m_net->sendCustomRequest(
            jsonReq(kWebhook + "/messages/" + m_pendingId), "PATCH", body(payload));
        m_pendingId.clear();
        connect(reply, &QNetworkReply::finished, this, [this, reply, payload] {
            reply->deleteLater();
            if (reply->error() != QNetworkReply::NoError) postRaw(payload);   // edicao falhou -> posta nova
        });
    } else {
        postRaw(payload);
    }
}

void DiscordAudit::postRaw(const QJsonObject& payload) {
    QNetworkReply* reply = m_net->post(jsonReq(kWebhook), body(payload));
    connect(reply, &QNetworkReply::finished, reply, &QNetworkReply::deleteLater);
}

}  // namespace sphone
