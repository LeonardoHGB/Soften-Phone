#pragma once
//
// callhistory.h — Historico de chamadas + estatisticas do dia (porte de
// CallHistory.cs). NESTA FASE e apenas em memoria; a persistencia em
// history.json (System.Text.Json equivalente) entra na fase 6.
//
#include "data/callaudit.h"
#include <QList>

namespace sphone {

struct DayStats {
    int incoming = 0;
    int outgoing = 0;
    int missed = 0;
    int answered = 0;
    int talkSeconds = 0;
    int total() const { return incoming + outgoing; }
};

class CallHistory {
public:
    static constexpr int MaxEntries = 200;
    static void add(const CallAudit& c);   // mais recente no indice 0; cap 200
    static QList<CallAudit> all();
    static DayStats today();
};

}  // namespace sphone
