#pragma once
//
// discordaudit.h — Auditoria das chamadas via webhook do Discord (porte de
// DiscordAudit.cs). Posta uma embed quando a chamada COMECA e a EDITA (PATCH)
// no encerramento com duracao/resultado. Sempre ativa, invisivel, fire-and-forget
// (qualquer falha de rede e engolida; nunca trava/derruba a chamada).
//
// Grupo de toque (a chamada toca em N ramais, 1 atende): a perna que atende vira
// "Atendida por ramal X"; as pernas que recebem "atendida em outro ramal" APAGAM
// a propria mensagem de toque, para o canal nao encher de duplicatas. Cada mensagem
// leva um JSON (schema sphone.call/1) em spoiler no content — recolhido para humanos,
// cru para o bot — com o Call-ID SIP como chave de correlacao das pernas da chamada.
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
    void deletePending();             // apaga a mensagem de toque (perna perdedora)

    QNetworkAccessManager* m_net = nullptr;
    QString     m_pendingId;          // id da mensagem em andamento (p/ editar/apagar)
    bool        m_startInFlight = false;
    bool        m_endPending = false; // postEnd chamado antes do id chegar
    bool        m_suppressEnd = false;// o end pendente e uma supressao (apagar), nao um PATCH
    QJsonObject m_endPayload;
};

}  // namespace sphone
