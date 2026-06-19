#include "audio/ringtone.h"
#include "core/diag.h"

#include <QMediaPlayer>
#include <QAudioOutput>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QUrl>

namespace sphone {

Ringtone::Ringtone(QObject* parent) : QObject(parent) {
    m_out = new QAudioOutput(this);
    m_out->setVolume(1.0);
    m_player = new QMediaPlayer(this);
    m_player->setAudioOutput(m_out);
    m_player->setLoops(QMediaPlayer::Infinite);   // loop continuo
    // Falhas de carga/codec deixam de ser silenciosas.
    connect(m_player, &QMediaPlayer::errorOccurred, this,
            [](QMediaPlayer::Error, const QString& msg) {
                sphone::diag::log("Ringtone: erro ao tocar -> " + msg);
            });
}

void Ringtone::start() {
    // Re-extrai se o cache sumiu (limpador de %TEMP% ou quarentena de AV do arquivo).
    if (m_path.isEmpty() || !QFile::exists(m_path)) {
        const QString tmp = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
                                .filePath(QStringLiteral("sphone-chamando.mp3"));
        QFile::remove(tmp);
        if (QFile::copy(QStringLiteral(":/assets/chamando.mp3"), tmp)) {
            QFile::setPermissions(tmp, QFile::ReadOwner | QFile::WriteOwner);
            m_path = tmp;
        } else {
            sphone::diag::log("Ringtone: falha ao extrair chamando.mp3 para " + tmp);
            m_path.clear();
            return;
        }
    }
    m_player->setSource(QUrl::fromLocalFile(m_path));
    m_player->play();
}

void Ringtone::stop() {
    if (m_player) m_player->stop();
}

}  // namespace sphone
