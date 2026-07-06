#pragma once
//
// mainwindow.h — Shell desktop "Signal Architecture".
//
// Janela frameless com TitleBar + (NavRail | Discador | Chamada | Recentes)
// visiveis juntos. A MainWindow e a CAMADA DE CONTROLE: orquestra o SipManager,
// timers, bandeja, saida por senha de supervisor, ring/flash e troca de tema a
// quente. Os paineis (ui/panels.h) sao apenas vista. O backend (SIP, audio,
// historico, config) e identico ao do widget de canto anterior.
//
#include <QWidget>
#include <QDateTime>

#include "sip/sipmanager.h"
#include "data/sipconfig.h"
#include "data/callaudit.h"

class QTimer;
class QSystemTrayIcon;

namespace sphone {

class Tones;
class Ringtone;
class DiscordAudit;
class TitleBar;
class NavRail;
class DialerPanel;
class CallPanel;
class RecentsPanel;
class SettingsPanel;

class MainWindow : public QWidget {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    void restoreFromTray();

protected:
    void closeEvent(QCloseEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void showEvent(QShowEvent*) override;
    void changeEvent(QEvent*) override;
#ifdef Q_OS_WIN
    bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;
#endif

private:
    void buildShell();
    void wirePanels();
    void rebuildShell();          // reconstroi a UI (troca de tema)
    void updateLayout();          // visibilidade dos paineis conforme estado/recentes
    void setWindowLocked(bool on);// trava janela (chamada recebida)
    void applyRoundedMask();
    void anchorBottomRight();     // cola a janela no canto inferior direito da tela

    // SIP / estado
    void startSip();
    void applyState(SipManager::LineState st);
    void startOutgoingCall();
    void sendDtmfIfInCall(QChar key);
    void updatePill();

    // timer da chamada
    void startCallTimer(); void stopCallTimer();
    void startMeter();     void stopMeter();      // alimenta o waveform
    void startStats();     void stopStats();      // telemetria do rodape

    // chamada recebida / janela
    void startRing(); void stopRing();
    void bringToForeground(); void keepRingingOnTop();
    void setTopMost(bool on); void flashWindow(bool start);

    // bandeja / saida / config / janela
    void buildTray();
    void hideToTray(); void tryExit(); void openSettings();

    // dados
    SipConfig   m_config;
    SipManager* m_sip = nullptr;

    // shell
    QWidget*      m_content = nullptr;
    TitleBar*     m_titleBar = nullptr;
    NavRail*      m_nav = nullptr;
    DialerPanel*  m_dialer = nullptr;
    CallPanel*    m_call = nullptr;
    RecentsPanel* m_recents = nullptr;
    SettingsPanel* m_settings = nullptr;
    QWidget*      m_sep = nullptr;          // divisoria discador|extra
    bool          m_recentsOpen = false;    // Recentes encaixado (toggle do nav)
    bool          m_settingsOpen = false;   // Configuracoes encaixado (toggle do nav)
    bool          m_windowLocked = false;   // chamada recebida trava a janela

    // estado de chamada
    QString   m_peerName, m_peerNumber;
    QTimer*   m_callTimer = nullptr; QDateTime m_callStart;
    QTimer*   m_meterTimer = nullptr; float m_levelShown = 0;
    QTimer*   m_statsTimer = nullptr;

    QSystemTrayIcon* m_tray = nullptr;
    Tones*        m_tones = nullptr;
    Ringtone*     m_ringtone = nullptr;
    DiscordAudit* m_discord = nullptr;

    bool m_ringing = false;
    bool m_reallyExit = false;
    bool m_balloonShown = false;
    bool m_registered = false;
    QString m_lastStatus = QStringLiteral("Iniciando…");

    SipManager::LineState m_prevState = SipManager::LineState::Offline;
};

}  // namespace sphone
