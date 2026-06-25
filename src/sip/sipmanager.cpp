#include "sip/sipmanager.h"
#include "sip/pjengine.h"

#include <QTimer>
#include <algorithm>

namespace sphone {

static constexpr int kQueueRingGraceMs = 30 * 1000;   // QueueRingGrace = 30s

SipManager::SipManager(const SipConfig& config, QObject* parent)
    : QObject(parent), m_config(config) {
    qRegisterMetaType<sphone::CallAudit>();
    qRegisterMetaType<sphone::SipManager::LineState>();

    m_pj = new PjEngine(this);
    // Os sinais do PjEngine vem de threads do PJSIP -> Queued para a thread de UI.
    connect(m_pj, &PjEngine::regState,     this, &SipManager::onRegState,     Qt::QueuedConnection);
    connect(m_pj, &PjEngine::callState,    this, &SipManager::onCallState,    Qt::QueuedConnection);
    connect(m_pj, &PjEngine::incomingCall, this, &SipManager::onIncomingCall, Qt::QueuedConnection);
}

SipManager::~SipManager() {
    clearPendingSession();
    if (m_started) {
        m_pj->unregister();
        m_pj->shutdown();
        m_started = false;
    }
}

void SipManager::setState(LineState s) {
    m_state = s;
    emit stateChanged(s);
}

void SipManager::start() {
    setState(LineState::Registering);
    emit statusMessage(QStringLiteral("Registrando ramal %1...").arg(m_config.username));

    int r = m_pj->start(0);
    if (r != 0) {
        emit statusMessage(QStringLiteral("Erro ao iniciar o PJSIP (codigo %1).").arg(r));
        setState(LineState::Offline);
        return;
    }
    m_started = true;

    int acc = m_pj->registerAccount(domain(), m_config.username, m_config.password);
    if (acc < 0) {
        emit statusMessage(QStringLiteral("Falha ao configurar o registro do ramal."));
        setState(LineState::Offline);
    }
}

// =====================================================================
//  Callbacks do PjEngine (ja na thread de UI via QueuedConnection)
// =====================================================================

void SipManager::onRegState(int registered, int code) {
    if (registered != 0) {
        emit statusMessage(QStringLiteral("Ramal registrado com sucesso."));
        emit registrationChanged(true);
        if (m_state == LineState::Registering || m_state == LineState::Offline)
            setState(LineState::Idle);
    } else {
        emit statusMessage(code > 0 ? QStringLiteral("Falha no registro (codigo %1).").arg(code)
                                    : QStringLiteral("Ramal nao registrado."));
        emit registrationChanged(false);
        setState(LineState::Offline);
    }
}

void SipManager::onIncomingCall(int callId, QString from, QString sipCallId) {
    auto [number, name] = parseCaller(from);
    QString num = number.trimmed().isEmpty() ? QStringLiteral("desconhecido") : number;

    // Resolve eventual sessao pendente (rodada de fila anterior).
    bool continuation = false;
    bool hasFinalizePrev = false;
    CallAudit finalizePrev;
    if (m_sessionPending) {
        if (num == m_pendingMissNumber) {
            continuation = true;            // mesma chamada, fila apenas avancou de rodada
        } else if (m_pendingMissValid) {
            finalizePrev = m_pendingMissRecord;   // outra chamada chegou -> a anterior foi perda
            hasFinalizePrev = true;
        }
        m_sessionPending = false;
        m_pendingMissValid = false;
        m_pendingMissNumber.clear();
        if (m_pendingMissTimer) m_pendingMissTimer->stop();
    }

    if (hasFinalizePrev)
        emit callEnded(finalizePrev);

    if (continuation) {
        m_incomingCall = callId;
        m_currentCall  = callId;
        m_callSipId    = sipCallId;   // rodada nova da fila: Call-ID pode ter mudado
        emit statusMessage(QStringLiteral("Chamada recebida de %1.").arg(m_incomingFrom));
        emit incomingCallSignal(m_incomingFrom, m_incomingName);
        setState(LineState::Ringing);
        return;
    }

    // ---- Nova chamada ----
    m_incomingCall = callId;
    m_currentCall  = callId;
    m_incomingFrom = num;
    m_incomingName = name;
    emit statusMessage(QStringLiteral("Chamada recebida de %1.").arg(m_incomingFrom));

    m_callDir = CallDirection::Inbound;
    m_callPeerNumber = m_incomingFrom;
    m_callPeerName   = m_incomingName;
    m_callSipId      = sipCallId;
    m_callStarted = QDateTime::currentDateTime(); m_callStartedValid = true;
    m_callAnsweredValid = false;
    m_callTransferred = false;
    raiseCallStarted();

    emit incomingCallSignal(m_incomingFrom, m_incomingName);
    setState(LineState::Ringing);
}

void SipManager::onCallState(int callId, int state, int lastCode, int flags) {
    if (state == PjEngine::InvCalling || state == PjEngine::InvEarly) {
        if (m_state == LineState::Calling)
            emit statusMessage(QStringLiteral("Chamando..."));
        return;
    }
    if (state == PjEngine::InvConfirmed) {
        clearPendingSession();
        m_currentCall  = callId;
        m_incomingCall = -1;
        m_muted = false;
        m_onHold = false;
        setState(LineState::InCall);
        m_callAnswered = QDateTime::currentDateTime(); m_callAnsweredValid = true;
        emit statusMessage(QStringLiteral("Em chamada."));
        return;
    }
    if (state == PjEngine::InvDisconnected) {
        bool elsewhere = (flags & PjEngine::FlagCompletedElsewhere) != 0;

        // Rodada de fila cancelada sem atender/recusar/elsewhere: pode ser so a fila
        // avancando. Abre sessao pendente em vez de marcar "Perdida" agora.
        if (m_state == LineState::Ringing && !elsewhere && !m_localHangup
            && m_callDir == CallDirection::Inbound && !m_callAnsweredValid) {
            beginPendingMiss();
            m_incomingCall = -1;
            m_currentCall  = -1;
            setState(LineState::Idle);
            return;
        }

        QString msg =
            elsewhere ? QStringLiteral("Atendida em outro ramal.")
          : m_state == LineState::Calling ? friendlyFailure(lastCode)
          : m_state == LineState::Ringing ? (m_localHangup ? QStringLiteral("Chamada recusada.")
                                                           : QStringLiteral("Chamada perdida."))
          : QStringLiteral("Chamada encerrada.");
        emit statusMessage(msg);
        raiseCallEnded(QString(), lastCode, elsewhere);
        cleanupCall();
        setState(LineState::Idle);
    }
}

QString SipManager::friendlyFailure(int code) {
    switch (code) {
        case 486: case 600: return QStringLiteral("Ramal ocupado.");
        case 603:           return QStringLiteral("Chamada recusada.");
        case 404: case 484: return QStringLiteral("Numero inexistente.");
        case 480: case 408: return QStringLiteral("Ramal indisponivel.");
        case 487:           return QStringLiteral("Chamada encerrada.");
        default:            return QStringLiteral("Nao foi possivel completar a chamada.");
    }
}

// =====================================================================
//  Acoes
// =====================================================================

void SipManager::answer() {
    int id = m_incomingCall;
    if (id < 0) return;
    m_incomingCall = -1;
    m_pj->answer(id);          // estado vira InCall pela callback CONFIRMED
    m_currentCall = id;
}

void SipManager::reject() {
    int id = m_incomingCall;
    m_incomingCall = -1;
    m_localHangup = true;
    if (id >= 0) m_pj->hangup(id);
}

void SipManager::setMute(bool mute) {
    if (m_currentCall >= 0) { m_pj->mute(m_currentCall, mute); m_muted = mute; }
}

void SipManager::setHold(bool hold) {
    if (m_currentCall < 0) return;
    if (hold) m_pj->hold(m_currentCall);
    else      m_pj->unhold(m_currentCall);
    m_onHold = hold;
}

QString SipManager::normalizeDestination(const QString& raw) {
    if (raw.trimmed().isEmpty()) return QString();
    QString s = raw.trimmed();
    for (const QString& sep : {QStringLiteral(" "), QStringLiteral("-"), QStringLiteral("("),
                               QStringLiteral(")"), QStringLiteral(".")})
        s.remove(sep);
    if (s.isEmpty() || s.length() > 32) return QString();
    for (QChar c : s) {
        bool ok = (c >= '0' && c <= '9') || c == '*' || c == '#' || c == '+';
        if (!ok) return QString();
    }
    // Numeros vindos do historico chegam em E.164 (+5517997887174). O PABX nao
    // roteia com o codigo de pais: remove o "+55" (ou "0055") deixando DDD+numero.
    if (s.startsWith(QStringLiteral("+55")))      s.remove(0, 3);
    else if (s.startsWith(QStringLiteral("0055"))) s.remove(0, 4);
    return s;
}

void SipManager::call(const QString& destination) {
    if (!m_started || destination.trimmed().isEmpty()) return;
    if (m_state != LineState::Idle) {
        emit statusMessage(QStringLiteral("Ja existe uma chamada em andamento."));
        return;
    }
    QString number = normalizeDestination(destination);
    if (number.isEmpty()) {
        emit statusMessage(QStringLiteral("Numero invalido."));
        return;
    }

    m_localHangup = false;
    setState(LineState::Calling);
    emit statusMessage(QStringLiteral("Discando para %1...").arg(destination));

    m_callDir = CallDirection::Outbound;
    m_callPeerNumber = number;
    m_callPeerName.clear();
    m_callSipId.clear();
    m_callStarted = QDateTime::currentDateTime(); m_callStartedValid = true;
    m_callAnsweredValid = false;
    m_callTransferred = false;

    int id = m_pj->makeCall(domain(), number);
    if (id < 0) {
        emit statusMessage(QStringLiteral("Nao foi possivel completar a ligacao."));
        raiseCallEnded(QStringLiteral("Falha"), 0);
        cleanupCall();
        setState(LineState::Idle);
    } else {
        m_currentCall = id;
        m_callSipId = m_pj->sipCallId(id);   // chamada recem-criada: Call-ID ja valido
        raiseCallStarted();
    }
}

void SipManager::hangup() {
    m_localHangup = true;
    if (m_currentCall >= 0) m_pj->hangup(m_currentCall);
    else                    m_pj->hangupAll();
}

bool SipManager::transfer(const QString& destination) {
    if (m_currentCall < 0 || m_state != LineState::InCall) return false;
    QString number = normalizeDestination(destination);
    if (number.isEmpty()) {
        emit statusMessage(QStringLiteral("Ramal de transferencia invalido."));
        return false;
    }
    emit statusMessage(QStringLiteral("Transferindo para %1...").arg(destination));
    int r = m_pj->transfer(m_currentCall, domain(), number);
    if (r != 0) {
        emit statusMessage(QStringLiteral("Transferencia recusada ou falhou."));
        return false;
    }
    // REFER enviado; NAO encerramos a nossa perna (um BYE prematuro derruba a
    // transferencia no Asterisk). O PABX encerra o dialogo -> DISCONNECTED -> Idle.
    emit statusMessage(QStringLiteral("Transferido para %1.").arg(destination));
    m_callTransferred = true;
    return true;
}

void SipManager::sendDtmf(quint8 digit) {
    if (m_currentCall >= 0 && m_state == LineState::InCall) {
        QString s = digit == 10 ? QStringLiteral("*")
                  : digit == 11 ? QStringLiteral("#")
                                : QString::number(digit);
        m_pj->sendDtmf(m_currentCall, s);
    }
}

// =====================================================================
//  Auxiliares
// =====================================================================

float SipManager::readLevel(bool mic) {
    if (m_currentCall < 0 || m_state != LineState::InCall) return 0.0f;
    int tx = 0, rx = 0;
    if (m_pj->getLevel(m_currentCall, &tx, &rx) == 0) {
        int v = mic ? tx : rx;
        float n = v / 160.0f;          // ~fala normal preenche boa parte da barra
        return n > 1.0f ? 1.0f : n;
    }
    return 0.0f;
}

MediaStats SipManager::mediaStats() {
    MediaStats s;
    if (m_currentCall < 0 || m_state != LineState::InCall) return s;
    s.valid = m_pj->getStats(m_currentCall, s.codec, s.clockRate, s.rttMs, s.lossPermil);
    return s;
}

QPair<QString, QString> SipManager::parseCaller(const QString& remote) {
    if (remote.trimmed().isEmpty()) return {QString(), QString()};

    QString name;
    int q1 = remote.indexOf('"');
    if (q1 >= 0) {
        int q2 = remote.indexOf('"', q1 + 1);
        if (q2 > q1) name = remote.mid(q1 + 1, q2 - q1 - 1).trimmed();
    }

    QString number;
    int lt = remote.indexOf('<');
    QString uriPart = lt >= 0 ? remote.mid(lt + 1) : remote;
    int sip = uriPart.indexOf(QStringLiteral("sip:"), 0, Qt::CaseInsensitive);
    if (sip >= 0) {
        int start = sip + 4;
        int at = uriPart.indexOf('@', start);
        int end;
        if (at >= 0) {
            end = at;
        } else {
            // IndexOfAny(['>', ';', ' ']): primeiro delimitador apos o numero.
            end = -1;
            for (int c : { uriPart.indexOf('>', start), uriPart.indexOf(';', start),
                           uriPart.indexOf(' ', start) })
                if (c >= 0 && (end < 0 || c < end)) end = c;
        }
        if (end < 0) end = uriPart.length();
        number = uriPart.mid(start, end - start).trimmed();
    }

    if (name.isEmpty() && lt > 0) {
        QString pre = remote.left(lt).trimmed();
        if (pre.startsWith('"')) pre = pre.mid(1);
        if (pre.endsWith('"')) pre.chop(1);
        pre = pre.trimmed();
        if (!pre.isEmpty()) name = pre;
    }

    if (name == number) name.clear();   // numero nao serve como nome
    return {number, name};
}

void SipManager::raiseCallStarted() {
    if (!m_callStartedValid) return;
    CallAudit rec;
    rec.direction   = m_callDir;
    rec.peerNumber  = m_callPeerNumber;
    rec.peerName    = m_callPeerName;
    rec.ramal       = m_config.username;
    rec.startedLocal = m_callStarted;
    rec.durationSeconds = 0;
    rec.answered = false;
    rec.answeredElsewhere = false;
    rec.outcome = m_callDir == CallDirection::Outbound ? QStringLiteral("Chamando")
                                                       : QStringLiteral("Tocando");
    rec.sipCallId = m_callSipId;
    emit callStarted(rec);
}

void SipManager::raiseCallEnded(const QString& forcedOutcome, int lastCode, bool answeredElsewhere) {
    if (!m_callStartedValid) return;

    bool answered = m_callAnsweredValid;
    int duration = answered
        ? std::max(0, int(m_callAnswered.secsTo(QDateTime::currentDateTime())))
        : 0;

    QString outcome =
        !forcedOutcome.isEmpty() ? forcedOutcome
      : answeredElsewhere ? QStringLiteral("Atendida em outro ramal")
      : m_callTransferred ? QStringLiteral("Transferida")
      : answered ? QStringLiteral("Atendida")
      : m_callDir == CallDirection::Outbound
            ? (m_localHangup ? QStringLiteral("Cancelada") : friendlyFailure(lastCode))
            : (m_localHangup ? QStringLiteral("Recusada")  : QStringLiteral("Perdida"));

    CallAudit rec;
    rec.direction   = m_callDir;
    rec.peerNumber  = m_callPeerNumber;
    rec.peerName    = m_callPeerName;
    rec.ramal       = m_config.username;
    rec.startedLocal = m_callStarted;
    rec.durationSeconds = duration;
    rec.answered = answered;
    rec.answeredElsewhere = answeredElsewhere;
    rec.outcome = outcome;
    rec.sipCallId = m_callSipId;

    m_callStartedValid = false;   // evita auditar duas vezes
    emit callEnded(rec);
}

void SipManager::cleanupCall() {
    m_currentCall = -1;
    m_incomingCall = -1;
    m_muted = false;
    m_onHold = false;
    m_localHangup = false;
    m_callStartedValid = false;
    m_callAnsweredValid = false;
    m_callTransferred = false;
    clearPendingSession();
}

// =====================================================================
//  Coalescing de rodadas de toque de fila
// =====================================================================

void SipManager::beginPendingMiss() {
    CallAudit rec;
    rec.direction = CallDirection::Inbound;
    rec.peerNumber = m_callPeerNumber;
    rec.peerName = m_callPeerName;
    rec.ramal = m_config.username;
    rec.startedLocal = m_callStartedValid ? m_callStarted : QDateTime::currentDateTime();
    rec.durationSeconds = 0;
    rec.answered = false;
    rec.answeredElsewhere = false;
    rec.outcome = QStringLiteral("Perdida");
    rec.sipCallId = m_callSipId;

    m_pendingMissRecord = rec;
    m_pendingMissValid = true;
    m_pendingMissNumber = m_callPeerNumber;
    m_sessionPending = true;

    if (!m_pendingMissTimer) {
        m_pendingMissTimer = new QTimer(this);
        m_pendingMissTimer->setSingleShot(true);
        connect(m_pendingMissTimer, &QTimer::timeout, this, &SipManager::finalizePendingMiss);
    }
    m_pendingMissTimer->start(kQueueRingGraceMs);
}

void SipManager::finalizePendingMiss() {
    if (!m_sessionPending) return;   // ja resolvida (atendida / re-toque / outra chamada)
    m_sessionPending = false;
    if (m_pendingMissTimer) m_pendingMissTimer->stop();
    if (!m_pendingMissValid) return;
    CallAudit rec = m_pendingMissRecord;
    m_pendingMissValid = false;
    m_pendingMissNumber.clear();
    emit statusMessage(QStringLiteral("Chamada perdida."));
    emit callEnded(rec);
}

void SipManager::clearPendingSession() {
    m_sessionPending = false;
    m_pendingMissValid = false;
    m_pendingMissNumber.clear();
    if (m_pendingMissTimer) m_pendingMissTimer->stop();
}

}  // namespace sphone
