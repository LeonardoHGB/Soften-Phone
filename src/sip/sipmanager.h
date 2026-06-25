#pragma once
//
// sipmanager.h — Maquina de estados SIP de alto nivel (porte de SipManager.cs).
//
// Conecta-se ao PjEngine (sinais re-marshallados para a thread de UI), mantem o
// LineState, emite mensagens de status PT-BR e eventos de auditoria, e implementa
// o coalescing de rodadas de fila (30s) e a deteccao "atendida em outro ramal".
// Como os sinais do PjEngine chegam via Qt::QueuedConnection na thread de UI,
// toda a logica aqui roda single-thread (sem locks, ao contrario do original).
//
#include <QObject>
#include <QString>
#include <QDateTime>
#include <QPair>

#include "data/sipconfig.h"
#include "data/callaudit.h"

class QTimer;

namespace sphone {

class PjEngine;

// Telemetria de midia da chamada ativa (rodape do painel Recentes).
struct MediaStats {
    bool    valid = false;
    QString codec;          // ex "opus", "PCMU"
    int     clockRate = 0;  // Hz (ex 48000)
    int     rttMs = -1;     // latencia RTT em ms (-1 = desconhecido)
    int     lossPermil = -1;// perda de pacotes RX em ‰ (-1 = desconhecido)
};

class SipManager : public QObject {
    Q_OBJECT
public:
    enum class LineState { Offline, Registering, Idle, Ringing, Calling, InCall };
    Q_ENUM(LineState)

    explicit SipManager(const SipConfig& config, QObject* parent = nullptr);
    ~SipManager() override;

    LineState state() const   { return m_state; }
    bool isMuted() const      { return m_muted; }
    bool isOnHold() const     { return m_onHold; }
    float micLevel()          { return readLevel(true); }
    float speakerLevel()      { return readLevel(false); }
    MediaStats mediaStats();  // telemetria da chamada ativa (invalida fora de InCall)

    void start();                         // inicia o PJSIP e registra o ramal
    void answer();                        // atende a chamada de entrada
    void reject();                        // recusa a chamada de entrada
    void setMute(bool mute);
    void setHold(bool hold);
    void call(const QString& destination);
    void hangup();
    bool transfer(const QString& destination);
    void sendDtmf(quint8 digit);          // 10=*, 11=#, senao digito

    // Limpa separadores e remove o codigo de pais "+55"; "" = numero invalido.
    static QString normalizeDestination(const QString& raw);

signals:
    void stateChanged(sphone::SipManager::LineState s);
    void statusMessage(const QString& msg);
    void incomingCallSignal(const QString& number, const QString& name);
    void registrationChanged(bool registered);
    void callStarted(const sphone::CallAudit& rec);
    void callEnded(const sphone::CallAudit& rec);

private slots:
    void onRegState(int registered, int code);
    void onCallState(int callId, int state, int lastCode, int flags);
    void onIncomingCall(int callId, QString from, QString sipCallId);

private:
    void  setState(LineState s);
    float readLevel(bool mic);
    static QString friendlyFailure(int code);
    static QPair<QString, QString> parseCaller(const QString& remote);
    void  raiseCallStarted();
    void  raiseCallEnded(const QString& forcedOutcome, int lastCode, bool answeredElsewhere = false);
    void  cleanupCall();
    void  beginPendingMiss();
    void  finalizePendingMiss();
    void  clearPendingSession();

    QString domain() const { return m_config.domain(); }

    SipConfig m_config;
    PjEngine* m_pj = nullptr;
    LineState m_state = LineState::Offline;
    bool m_muted = false, m_onHold = false;

    int     m_currentCall = -1, m_incomingCall = -1;
    QString m_incomingFrom, m_incomingName;
    bool    m_started = false;
    bool    m_localHangup = false;

    // Metadados da chamada em andamento (para auditoria).
    CallDirection m_callDir = CallDirection::Inbound;
    QString  m_callPeerNumber, m_callPeerName;
    QString  m_callSipId;      // Call-ID SIP da chamada corrente (correlacao p/ o bot)
    QDateTime m_callStarted;   bool m_callStartedValid = false;
    QDateTime m_callAnswered;  bool m_callAnsweredValid = false;
    bool m_callTransferred = false;

    // Coalescing de rodadas de fila (tudo na thread de UI).
    QTimer*   m_pendingMissTimer = nullptr;
    CallAudit m_pendingMissRecord; bool m_pendingMissValid = false;
    QString   m_pendingMissNumber;
    bool      m_sessionPending = false;
};

}  // namespace sphone
