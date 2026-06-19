#pragma once
//
// mainwindow.h — Janela principal e controladora (porte de MainForm.cs).
//
// Janela 360x560 travada no canto inferior direito, com 4 views sobrepostas
// (Dialer/Incoming/InCall/History), bandeja, saida com senha de supervisor e a
// maquina ApplyState ligada ao SipManager. Audio (toque/ringback) entra na
// fase 7; SettingsForm/UpdateForm e persistencia (config/Discord) nas fases 5b/6.
//
#include <QWidget>
#include <QPixmap>
#include <QDateTime>

#include "sip/sipmanager.h"
#include "data/sipconfig.h"
#include "data/callaudit.h"
#include "ui/controls.h"   // StatusLevel + tipos dos controles custom

class QLabel;
class QLineEdit;
class QTimer;
class QSystemTrayIcon;
class QVBoxLayout;

namespace sphone {

class Tones;
class Ringtone;
class DiscordAudit;

class MainWindow : public QWidget {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    void restoreFromTray();

protected:
    void closeEvent(QCloseEvent*) override;
    bool eventFilter(QObject* watched, QEvent* event) override;   // tom DTMF ao digitar
#ifdef Q_OS_WIN
    bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;
#endif

private:
    // construcao das telas
    void buildViews();
    QWidget* buildDialerView();
    QWidget* buildIncomingView();
    QWidget* buildInCallView();
    QWidget* buildHistoryView();
    QWidget* buildDialpad();
    void buildTray();
    void showView(QWidget* v);
    void rebuildViews();   // reconstroi as 4 telas (troca de tema)

    // posicao travada
    void lockToCorner();
    QPoint lockedPos() const;
    QPixmap whiteLogo() const;

    // SIP / estado
    void startSip();
    void applyState(SipManager::LineState st);
    void resetInCallControls();
    void setStatus(const QString& msg);
    void setStatusLevel(StatusLevel lvl);
    void startOutgoingCall();
    void sendDtmfIfInCall(const QString& key);
    void appendToDisplay(const QString& key);

    // timers
    void startCallTimer(); void stopCallTimer();
    void startMeter();     void stopMeter();

    // historico
    void showHistory(); void refreshHistory();
    QWidget* buildHistoryRow(const CallAudit& c);
    void redialFromHistory(const QString& number);

    // chamada recebida / janela
    void startRing(); void stopRing();
    void bringToForeground(); void keepRingingOnTop();
    void setTopMost(bool on); void flashWindow(bool start);

    // bandeja / saida / config
    void hideToTray(); void tryExit(); void openSettings();

    // dados
    SipConfig    m_config;
    SipManager*  m_sip = nullptr;

    QWidget* m_dialerView = nullptr;
    QWidget* m_incomingView = nullptr;
    QWidget* m_inCallView = nullptr;
    QWidget* m_historyView = nullptr;

    // dialer
    QLabel*          m_stateDot = nullptr;
    QLabel*          m_ramalLabel = nullptr;
    QLineEdit*       m_display = nullptr;
    RoundedCard*     m_displayCard = nullptr;
    ActionButton*    m_ligarBtn = nullptr;
    StatusIndicator* m_statusIndicator = nullptr;

    // incoming
    QLabel* m_incName = nullptr;
    QLabel* m_incNumber = nullptr;

    // in call
    QLabel*      m_callName = nullptr;
    QLabel*      m_callTimerLabel = nullptr;
    CallControl* m_muteCtrl = nullptr;
    CallControl* m_holdCtrl = nullptr;
    LevelBar*    m_micBar = nullptr;
    LevelBar*    m_speakerBar = nullptr;

    // historico
    QVBoxLayout* m_historyListLayout = nullptr;
    QLabel* m_statIn = nullptr;
    QLabel* m_statOut = nullptr;
    QLabel* m_statMissed = nullptr;
    QLabel* m_statTime = nullptr;

    // estado de chamada
    QString   m_peerName, m_peerNumber;
    QTimer*   m_callTimer = nullptr;  QDateTime m_callStart;
    QTimer*   m_meterTimer = nullptr; float m_micShown = 0, m_spkShown = 0;
    QTimer*   m_posGuard = nullptr;

    QSystemTrayIcon* m_tray = nullptr;
    Tones*        m_tones = nullptr;
    Ringtone*     m_ringtone = nullptr;
    DiscordAudit* m_discord = nullptr;
    bool m_ringing = false;
    bool m_reallyExit = false;
    bool m_balloonShown = false;

    QString     m_lastStatus = QStringLiteral("Iniciando...");
    StatusLevel m_lastLevel = StatusLevel::Warn;
    SipManager::LineState m_prevState = SipManager::LineState::Offline;   // p/ bipes iniciar/encerrar
};

}  // namespace sphone
