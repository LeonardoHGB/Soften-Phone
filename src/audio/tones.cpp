#include "audio/tones.h"

#include <QAudioSink>
#include <QAudioDevice>
#include <QMediaDevices>
#include <QBuffer>
#include <QtMath>
#include <QAudio>
#include <QTimer>

namespace sphone {

static constexpr int kRate = 44100;

Tones::Tones(QObject* parent) : QObject(parent) {
    m_fmt.setSampleRate(kRate);
    m_fmt.setChannelCount(1);
    m_fmt.setSampleFormat(QAudioFormat::Int16);

    struct D { char k; int low, high; };
    static const D table[] = {
        {'1', 697, 1209}, {'2', 697, 1336}, {'3', 697, 1477},
        {'4', 770, 1209}, {'5', 770, 1336}, {'6', 770, 1477},
        {'7', 852, 1209}, {'8', 852, 1336}, {'9', 852, 1477},
        {'*', 941, 1209}, {'0', 941, 1336}, {'#', 941, 1477},
        {'+', 941, 1336},
    };
    for (const D& d : table)
        m_dtmf.insert(QChar(d.k), synth({ d.low, d.high }, 140, 0.28, 0));

    // Ringback: 1s de tom 425 Hz + 4s de silencio, repetido em loop.
    m_ringbackPcm = synth({ 425 }, 1000, 0.22, 4000);
    // Efeitos discretos: 1 bipe agudo ao iniciar a chamada; 2 bipes graves ao encerrar.
    m_startPcm = synthBeeps(660, 110, 0, 1, 0.30);
    m_endPcm   = synthBeeps(440, 130, 90, 2, 0.30);

    const QAudioDevice dev = QMediaDevices::defaultAudioOutput();
    m_dtmfSink = new QAudioSink(dev, m_fmt, this);
    m_dtmfBuf  = new QBuffer(this);
    m_rbSink   = new QAudioSink(dev, m_fmt, this);
    m_rbBuf    = new QBuffer(this);
    m_rbBuf->setData(m_ringbackPcm);
    m_fxSink   = new QAudioSink(dev, m_fmt, this);
    m_fxBuf    = new QBuffer(this);

    // Loop do ringback: ao esgotar o QBuffer (Idle), reinicia do inicio. O gap cai
    // na cauda dos 4s de silencio -> imperceptivel. (O QAudioSink puxa bem de
    // QBuffer; um QIODevice sequencial custom nao era puxado no Windows -> silencio.)
    connect(m_rbSink, &QAudioSink::stateChanged, this, [this](QAudio::State s) {
        if (m_ringbackOn && s == QAudio::IdleState)
            QTimer::singleShot(0, this, [this] {
                if (m_ringbackOn) { m_rbBuf->seek(0); m_rbSink->start(m_rbBuf); }
            });
    });
}

Tones::~Tones() { stopRingback(); }

QByteArray Tones::synth(const QList<int>& freqs, int toneMs, double amplitude, int silenceMs) {
    const int toneN = kRate * toneMs / 1000;
    const int silN  = kRate * silenceMs / 1000;
    const int n     = toneN + silN;
    QByteArray pcm(n * 2, '\0');                 // int16 LE; o silencio fica em zeros
    auto* s = reinterpret_cast<qint16*>(pcm.data());

    const int fade = kRate / 200;                // ~5ms p/ evitar "clique"
    for (int i = 0; i < toneN; i++) {
        const double t = double(i) / kRate;
        double v = 0;
        for (int f : freqs) v += std::sin(2.0 * M_PI * f * t);
        v /= freqs.size();

        double env = 1.0;
        if (i < fade)                env = double(i) / fade;
        else if (i >= toneN - fade)  env = double(toneN - 1 - i) / fade;   // zera na ultima amostra

        s[i] = qint16(v * amplitude * 32767.0 * env);
    }
    return pcm;
}

// Sequencia de bipes (tom + gap, repetidos) — usado nos efeitos iniciar/encerrar.
QByteArray Tones::synthBeeps(int freq, int beepMs, int gapMs, int count, double amplitude) {
    const int beepN = kRate * beepMs / 1000;
    const int gapN  = kRate * gapMs / 1000;
    const int n     = count * beepN + (count > 0 ? (count - 1) * gapN : 0);
    QByteArray pcm(n * 2, '\0');
    auto* s = reinterpret_cast<qint16*>(pcm.data());

    const int fade = kRate / 200;
    int idx = 0;
    for (int b = 0; b < count; b++) {
        for (int i = 0; i < beepN; i++) {
            const double t = double(i) / kRate;
            const double v = std::sin(2.0 * M_PI * freq * t);
            double env = 1.0;
            if (i < fade)                env = double(i) / fade;
            else if (i >= beepN - fade)  env = double(beepN - 1 - i) / fade;
            s[idx++] = qint16(v * amplitude * 32767.0 * env);
        }
        if (b < count - 1) idx += gapN;   // gap = silencio (zeros)
    }
    return pcm;
}

void Tones::playDtmf(QChar key) {
    const auto it = m_dtmf.constFind(key);
    if (it == m_dtmf.constEnd()) return;
    m_dtmfSink->stop();
    m_dtmfBuf->close();
    m_dtmfBuf->setData(*it);
    m_dtmfBuf->open(QIODevice::ReadOnly);
    m_dtmfBuf->seek(0);
    m_dtmfSink->start(m_dtmfBuf);
}

void Tones::startRingback() {
    if (m_ringbackOn) return;
    m_ringbackOn = true;
    if (!m_rbBuf->isOpen()) m_rbBuf->open(QIODevice::ReadOnly);
    m_rbBuf->seek(0);
    m_rbSink->start(m_rbBuf);
}

void Tones::stopRingback() {
    if (!m_ringbackOn) return;
    m_ringbackOn = false;
    if (m_rbSink) m_rbSink->stop();
}

void Tones::playOnceFx(const QByteArray& pcm) {
    m_fxSink->stop();
    m_fxBuf->close();
    m_fxBuf->setData(pcm);
    m_fxBuf->open(QIODevice::ReadOnly);
    m_fxBuf->seek(0);
    m_fxSink->start(m_fxBuf);
}

void Tones::playStart() { playOnceFx(m_startPcm); }
void Tones::playEnd()   { playOnceFx(m_endPcm); }

}  // namespace sphone
