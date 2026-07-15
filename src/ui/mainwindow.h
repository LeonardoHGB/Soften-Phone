#pragma once
//
// mainwindow.h — Shell compacto estilo MicroSIP.
//
// Janela frameless UNICA de tamanho FIXO: TitleBar + TabsBar (Telefone /
// Registros / Config) + QStackedWidget com as paginas + StatusBar. A MainWindow
// e a CAMADA DE CONTROLE: orquestra o SipManager, timers, bandeja, saida por
// senha de supervisor, ring/flash e a troca de paginas. As paginas
// (ui/panels.h) sao apenas vista. O backend (SIP, audio, historico, config) e
// identico ao do shell anterior.
//
#include <QWidget>
#include <QDateTime>

#include "sip/sipmanager.h"
#include "data/sipconfig.h"
#include "data/callaudit.h"

class QTimer;
class QSystemTrayIcon;
class QStackedWidget;

namespace sphone {

class Tones;
class Ringtone;
class DiscordAudit;
class TitleBar;
class TabsBar;
class StatusBar;
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
    void onTabClicked(int idx);   // clique nas abas (Telefone/Registros/Config)
    void updateLayout();          // escolhe a pagina do stack conforme estado/aba
    void setWindowLocked(bool on);// trava janela (chamada recebida)
    void applyRoundedMask();
    void anchorBottomRight();     // cola a janela no canto inferior direito da tela

    // SIP / estado
    void startSip();
    void applyState(SipManager::LineState st);
    void startOutgoingCall();
    void sendDtmfIfInCall(QChar key);
    void updateStatus();          // alimenta a barra de status inferior

    // timer da chamada
    void startCallTimer(); void stopCallTimer();
    void startStats();     void stopStats();      // telemetria do rodape de Registros

    // chamada recebida / janela
    void startRing(); void stopRing();
    void startAutoAnswer(); void stopAutoAnswer();   // atende sozinho apos o toque
    void bringToForeground(); void keepRingingOnTop();
    void setTopMost(bool on); void flashWindow(bool start);

    // bandeja / saida / config / janela
    void buildTray();
    void hideToTray(); void tryExit(); void openSettings();

    // dados
    SipConfig   m_config;
    SipManager* m_sip = nullptr;

    // shell
    TitleBar*      m_titleBar = nullptr;
    TabsBar*       m_tabsBar = nullptr;
    StatusBar*     m_status = nullptr;
    QStackedWidget* m_stack = nullptr;
    DialerPanel*   m_dialer = nullptr;
    CallPanel*     m_call = nullptr;
    RecentsPanel*  m_recents = nullptr;
    SettingsPanel* m_settings = nullptr;
    int            m_tab = 0;               // aba ativa (0 Telefone / 1 Registros / 2 Config)
    bool           m_dtmfPad = false;       // em chamada: mostra o discador p/ DTMF
    bool           m_windowLocked = false;  // chamada recebida trava a janela

    // estado de chamada
    QString   m_peerName, m_peerNumber;
    QTimer*   m_callTimer = nullptr; QDateTime m_callStart;
    QTimer*   m_statsTimer = nullptr;
    QTimer*   m_autoAnswerTimer = nullptr;

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
