#pragma once
//
// exitaudit.h — Auditoria de encerramento do app via webhook dedicado do Discord
// (SPHONE_DISCORD_EXIT_WEBHOOK em secret.h, separado do webhook das chamadas).
//
// Posta um aviso quando alguem ENCERRA o Soften Phone (ou TENTA, com senha de
// supervisor errada). O envio e BLOQUEANTE com timeout curto, porque o processo
// costuma sair logo em seguida — sem bloquear, o POST seria cortado.
//
#include <QString>

namespace sphone {

class ExitAudit {
public:
    enum Event {
        ExitAttemptDenied,   // tentou encerrar, senha de supervisor incorreta
        ExitBySupervisor,    // encerrado com senha de supervisor correta
        AppQuit,             // encerramento generico (ex.: logoff/shutdown do Windows)
    };

    // Posta o evento no webhook de encerramento. Bloqueia ate enviar ou ate o
    // timeout (~3s). No-op se o webhook nao estiver configurado.
    static void report(Event ev, const QString& ramal);
};

}  // namespace sphone
