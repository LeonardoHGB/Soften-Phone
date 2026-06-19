#pragma once
//
// ringtone.h — Toque de chamada recebida (chamando.mp3 em loop), via Qt
// Multimedia (substitui o WPF MediaPlayer do original). O MP3 embutido e
// extraido para um arquivo temporario porque o QMediaPlayer toca de URL/arquivo.
//
#include <QObject>
#include <QString>

class QMediaPlayer;
class QAudioOutput;

namespace sphone {

class Ringtone : public QObject {
    Q_OBJECT
public:
    explicit Ringtone(QObject* parent = nullptr);
    void start();   // toca em loop
    void stop();

private:
    QMediaPlayer* m_player = nullptr;
    QAudioOutput* m_out = nullptr;
    QString       m_path;
};

}  // namespace sphone
