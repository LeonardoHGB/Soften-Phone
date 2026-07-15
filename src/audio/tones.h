#pragma once
//
// tones.h — Tons de telefone sintetizados em memoria (porte de Audio.cs/Tones).
// DTMF (one-shot) e ringback (425 Hz, 1s tom + 4s silencio, em loop). PCM mono
// 16-bit 44100 Hz tocado via QAudioSink (sem arquivos, sem WAV header).
//
#include <QObject>
#include <QHash>
#include <QByteArray>
#include <QAudioFormat>
#include <QList>

class QAudioSink;
class QBuffer;
class QIODevice;

namespace sphone {

class Tones : public QObject {
    Q_OBJECT
public:
    explicit Tones(QObject* parent = nullptr);
    ~Tones() override;

    void playDtmf(QChar key);     // 0-9 * # +
    void startRingback();         // cadencia BR em loop
    void stopRingback();
    void playStart();             // bipe curto ao INICIAR a chamada de saida
    void playEnd();               // bipe duplo ao ENCERRAR a chamada
    void playAutoAnswer();        // chime ascendente: chamada ATENDIDA sozinha

private:
    static QByteArray synth(const QList<int>& freqs, int toneMs, double amplitude, int silenceMs);
    static QByteArray synthBeeps(int freq, int beepMs, int gapMs, int count, double amplitude);
    void playOnceFx(const QByteArray& pcm);

    QAudioFormat             m_fmt;
    QHash<QChar, QByteArray> m_dtmf;       // PCM pre-sintetizado por tecla
    QAudioSink*              m_dtmfSink = nullptr;
    QBuffer*                 m_dtmfBuf = nullptr;
    QByteArray               m_ringbackPcm;
    QAudioSink*              m_rbSink = nullptr;
    QBuffer*                 m_rbBuf = nullptr;     // QBuffer (puxado pelo QAudioSink); loop por restart no Idle
    bool                     m_ringbackOn = false;
    QByteArray               m_startPcm, m_endPcm, m_autoPcm;   // efeitos one-shot
    QAudioSink*              m_fxSink = nullptr;
    QBuffer*                 m_fxBuf = nullptr;
};

}  // namespace sphone
