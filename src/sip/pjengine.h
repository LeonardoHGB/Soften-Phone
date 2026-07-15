#pragma once
//
// pjengine.h — Motor SIP nativo sobre a API C de alto nivel do PJSIP (pjsua-lib).
//
// Porte direto de native/pjcore.c para dentro do app (sem DLL/P-Invoke). Os
// callbacks C do pjsua rodam em threads do PJSIP e sao re-emitidos como sinais
// Qt; conecte-os com Qt::QueuedConnection para que cheguem na thread de UI.
//
// O corpo real (pjsua-lib) so compila com SPHONE_WITH_PJSIP=ON; sem isso, vira
// um stub no-op (permite buildar o shell visual antes do PJSIP estar pronto).
//
#include <QObject>
#include <QString>
#include <QList>

namespace sphone {

// Um dispositivo de audio enumerado do PJSIP. Um mesmo device pode servir para
// captura E reproducao (capture && playback), ou so um dos dois.
struct AudioDevice {
    int     id = -1;          // indice PJMEDIA (informativo; a config grava o nome)
    QString name;
    bool    capture  = false; // tem entrada (microfone)
    bool    playback = false; // tem saida (alto-falante/fone)
};

class PjEngine : public QObject {
    Q_OBJECT
public:
    explicit PjEngine(QObject* parent = nullptr);
    ~PjEngine() override;

    // Subconjunto de pjsip_inv_state usado pela maquina de estados.
    enum InvState { InvCalling = 1, InvEarly = 3, InvConfirmed = 5, InvDisconnected = 6 };
    enum Flags    { FlagCompletedElsewhere = 0x1 };

    int  start(int port);                 // 0 ok / -1 create / -2 init / -3 transport / -4 start
    int  registerAccount(const QString& domain, const QString& user, const QString& passwd); // acc>=0 / -1
    void unregister();
    void shutdown();

    int  makeCall(const QString& domain, const QString& dest);  // call_id / -1
    void answer(int callId);              // 200 OK
    void hangup(int callId);
    void busy(int callId);                // recusa com 486 Busy Here (linha ocupada)
    void hangupAll();
    void sendDtmf(int callId, const QString& digits);
    void hold(int callId);
    void unhold(int callId);
    int  transfer(int callId, const QString& domain, const QString& dest); // 0 / -1 (REFER)
    void mute(int callId, bool mute);
    int  getLevel(int callId, int* tx, int* rx);  // 0 ok / -1 (0..255)

    // Dispositivos de audio. audioDevices() faz re-scan e lista o que o SO expoe
    // (vazio se o motor nao esta iniciado). setSoundDevices() resolve os NOMES para
    // indices e aplica; nome vazio / nao encontrado cai no padrao do sistema.
    QList<AudioDevice> audioDevices();
    void setSoundDevices(const QString& captureName, const QString& playbackName);

    // Call-ID SIP da chamada (string do cabecalho). Vazio se indisponivel. Em
    // grupos de toque com proxy que forka, todas as pernas veem o mesmo valor.
    QString sipCallId(int callId);

    // Estatisticas de midia da chamada ativa (codec/RTT/perda RX). true se obteve;
    // out-params: codec (ex "opus"), clockRate (Hz), rttMs (-1 desconhecido),
    // lossPermil (perda RX em ‰, -1 desconhecido). Stub OFF retorna false.
    bool getStats(int callId, QString& codec, int& clockRate, int& rttMs, int& lossPermil);

    // Internos: chamados pelas callbacks C do pjsua para re-emitir os sinais.
    void emitReg(int registered, int code)                       { emit regState(registered, code); }
    void emitCallState(int callId, int st, int code, int flags)  { emit callState(callId, st, code, flags); }
    void emitIncoming(int callId, const QString& from, const QString& sipCallId) { emit incomingCall(callId, from, sipCallId); }

signals:
    void regState(int registered, int code);
    void callState(int callId, int state, int lastCode, int flags);
    void incomingCall(int callId, QString from, QString sipCallId);
};

}  // namespace sphone
