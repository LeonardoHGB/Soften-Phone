#pragma once
//
// discordaudit.h — Auditoria das chamadas via webhook do Discord (porte de
// DiscordAudit.cs). Posta uma embed quando a chamada COMECA e a EDITA (PATCH)
// no encerramento com duracao/resultado. Sempre ativa, invisivel, fire-and-forget
// (qualquer falha de rede e engolida; nunca trava/derruba a chamada).
//
#include <QObject>
#include <QString>
#include <QJsonObject>

#include "data/callaudit.h"

class QNetworkAccessManager;

namespace sphone {

class DiscordAudit : public QObject {
    Q_OBJECT
public:
    explicit DiscordAudit(QObject* parent = nullptr);

    void postStart(const sphone::CallAudit& c);   // chamada comecou
    void postEnd(const sphone::CallAudit& c);      // chamada terminou

private:
    QJsonObject buildPayload(const CallAudit& c, bool inProgress) const;
    void sendEnd(const QJsonObject& payload);
    void postRaw(const QJsonObject& payload);

    QNetworkAccessManager* m_net = nullptr;
    QString     m_pendingId;          // id da mensagem em andamento (p/ editar)
    bool        m_startInFlight = false;
    bool        m_endPending = false; // postEnd chamado antes do id chegar
    QJsonObject m_endPayload;
};

}  // namespace sphone
