#pragma once
//
// callaudit.h — Registro de auditoria de uma chamada (porte de CallAudit.cs).
// 9 campos, mesma ordem/semantica do original. Usado pelo historico e pelo
// webhook do Discord (fase 6).
//
#include <QString>
#include <QDateTime>
#include <QMetaType>

namespace sphone {

enum class CallDirection { Inbound = 0, Outbound = 1 };

struct CallAudit {
    CallDirection direction = CallDirection::Inbound;
    QString       peerNumber;          // numero de quem ligou / discado
    QString       peerName;            // display do PABX (pode vazio)
    QString       ramal;               // ramal deste cliente
    QDateTime     startedLocal;        // hora local de inicio
    int           durationSeconds = 0; // 0 se nao atendida
    bool          answered = false;    // conectou
    bool          answeredElsewhere = false; // atendida em outro ramal / ring group
    QString       outcome;             // "Atendida"/"Perdida"/"Recusada"/"Transferida"/"Cancelada"/...
    QString       sipCallId;           // Call-ID SIP: chave de correlacao das pernas
                                       // da mesma chamada (toca em N ramais) p/ o bot
};

}  // namespace sphone

Q_DECLARE_METATYPE(sphone::CallAudit)
