#include "data/callhistory.h"
#include "core/paths.h"

#include <QDate>
#include <QMutex>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

namespace sphone {

static QMutex          g_gate;
static QList<CallAudit>* g_cache = nullptr;   // null = ainda nao carregado

static CallAudit fromJson(const QJsonObject& o) {
    CallAudit c;
    c.direction = o.value("Direction").toInt() == 1 ? CallDirection::Outbound : CallDirection::Inbound;
    c.peerNumber = o.value("PeerNumber").toString();
    c.peerName = o.value("PeerName").toString();
    c.ramal = o.value("Ramal").toString();
    c.startedLocal = QDateTime::fromString(o.value("StartedLocal").toString(), Qt::ISODate);
    c.durationSeconds = o.value("DurationSeconds").toInt();
    c.answered = o.value("Answered").toBool();
    c.answeredElsewhere = o.value("AnsweredElsewhere").toBool();
    c.outcome = o.value("Outcome").toString();
    c.sipCallId = o.value("SipCallId").toString();
    return c;
}

static QJsonObject toJson(const CallAudit& c) {
    return QJsonObject{
        { "Direction", int(c.direction) },
        { "PeerNumber", c.peerNumber },
        { "PeerName", c.peerName },
        { "Ramal", c.ramal },
        { "StartedLocal", c.startedLocal.toString(Qt::ISODate) },
        { "DurationSeconds", c.durationSeconds },
        { "Answered", c.answered },
        { "AnsweredElsewhere", c.answeredElsewhere },
        { "Outcome", c.outcome },
        { "SipCallId", c.sipCallId },
    };
}

// Carrega o cache do disco na 1a vez (sob g_gate).
static QList<CallAudit>& loadLocked() {
    if (g_cache) return *g_cache;
    g_cache = new QList<CallAudit>();
    QFile f(paths::historyFile());
    if (f.open(QIODevice::ReadOnly)) {
        const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
        if (doc.isArray())
            for (const QJsonValue& v : doc.array())
                if (v.isObject()) g_cache->append(fromJson(v.toObject()));
    }
    return *g_cache;
}

static void saveLocked() {
    QJsonArray arr;
    for (const CallAudit& c : *g_cache) arr.append(toJson(c));
    QFile f(paths::historyFile());   // paths::dataDir() ja garante a pasta
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        f.write(QJsonDocument(arr).toJson(QJsonDocument::Compact));
    // best-effort: o historico nunca pode quebrar a chamada
}

void CallHistory::add(const CallAudit& c) {
    QMutexLocker lock(&g_gate);
    QList<CallAudit>& g = loadLocked();
    g.prepend(c);
    while (g.size() > MaxEntries) g.removeLast();
    saveLocked();
}

QList<CallAudit> CallHistory::all() {
    QMutexLocker lock(&g_gate);
    return loadLocked();
}

DayStats CallHistory::today() {
    QMutexLocker lock(&g_gate);
    DayStats s;
    const QDate today = QDate::currentDate();
    for (const CallAudit& c : loadLocked()) {
        if (c.startedLocal.date() != today) continue;
        if (c.direction == CallDirection::Inbound) {
            if (c.answeredElsewhere) continue;   // atendida em outro ramal -> ignorada
            s.incoming++;
            if (!c.answered) s.missed++;
        } else {
            s.outgoing++;
        }
        if (c.answered) { s.answered++; s.talkSeconds += c.durationSeconds; }
    }
    return s;
}

}  // namespace sphone
