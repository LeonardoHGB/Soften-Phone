#include "ui/mainwindow.h"
#include "ui/panels.h"
#include "ui/promptdialog.h"
#include "ui/updateform.h"
#include "core/brand.h"
#include "core/version.h"
#include "core/diag.h"
#include "data/callhistory.h"
#include "data/discordaudit.h"
#include "data/exitaudit.h"
#include "audio/tones.h"
#include "audio/ringtone.h"

#include <QVBoxLayout>
#include <QStackedWidget>
#include <QTimer>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QApplication>
#include <QScreen>
#include <QIcon>
#include <QMessageBox>
#include <QCryptographicHash>
#include <QCloseEvent>
#include <QResizeEvent>
#include <QShowEvent>
#include <QPainter>
#include <QPainterPath>
#include <QEvent>
#include <algorithm>

#ifdef Q_OS_WIN
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#  include <dwmapi.h>
#endif

using namespace brand;
using LS = sphone::SipManager::LineState;

#if __has_include("data/secret.h")
#  include "data/secret.h"
#else
#  include "data/secret.example.h"
#endif

namespace {
constexpr const char* kSupervisorExitHash = SPHONE_SUPERVISOR_HASH;
constexpr const char* kAppVersion = SPHONE_VERSION;
}

namespace sphone {

MainWindow::MainWindow(QWidget* parent) : QWidget(parent) {
    m_config = SipConfig::load();
    m_config.darkTheme = true;   // tema fixo grafite (toggle claro/escuro removido)
    setWindowTitle(QStringLiteral("Soften Phone"));
    setWindowIcon(QIcon(":/assets/logo.ico"));
    setWindowFlag(Qt::FramelessWindowHint, true);
    // Janela UNICA de tamanho FIXO (estilo MicroSIP): nenhum estado redimensiona.
    setFixedSize(dim::ShellW, dim::ShellH);

    applyTheme(m_config.darkTheme);

    m_tones = new Tones(this);
    m_ringtone = new Ringtone(this);
    m_discord = new DiscordAudit(this);

    buildShell();
    buildTray();
    anchorBottomRight();

    sphone::diag::log("MainWindow (shell compacto) criada.");

    QTimer::singleShot(0, this, [this] {
        if (!m_config.isComplete()) {
            m_lastStatus = QStringLiteral("Configuração incompleta");
            m_tab = TabsBar::Settings;   // abre direto na aba Config
            m_settings->loadConfig();
            updateLayout();
            updateStatus();
            return;                      // o registro inicia ao salvar (saved -> openSettings)
        }
        if (!m_sip) startSip();
    });
    QTimer::singleShot(4000, this, [this] { runUpdateCheck(this, /*silent*/ true); });
}

MainWindow::~MainWindow() {
    stopRing();
    stopAutoAnswer();
    stopCallTimer();
    if (m_sip) delete m_sip;
}

// =====================================================================
//  Construcao do shell
// =====================================================================
void MainWindow::buildShell() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    m_titleBar = new TitleBar(this);
    root->addWidget(m_titleBar);

    m_tabsBar = new TabsBar(this);
    root->addWidget(m_tabsBar);

    // Paginas empilhadas: todas ocupam a MESMA area (nada encaixa ao lado).
    m_stack = new QStackedWidget(this);
    m_dialer   = new DialerPanel(m_stack);
    m_call     = new CallPanel(m_stack);
    m_recents  = new RecentsPanel(m_stack);
    m_settings = new SettingsPanel(&m_config, m_stack);
    m_stack->addWidget(m_dialer);
    m_stack->addWidget(m_call);
    m_stack->addWidget(m_recents);
    m_stack->addWidget(m_settings);
    root->addWidget(m_stack, 1);

    m_status = new StatusBar(this);
    root->addWidget(m_status);

    wirePanels();
    updateLayout();   // estado inicial: pagina Telefone
    updateStatus();
}

void MainWindow::wirePanels() {
    connect(m_titleBar, &TitleBar::closeClicked, this, [this] { close(); });
    connect(m_tabsBar, &TabsBar::tabClicked, this, [this](int i) { onTabClicked(i); });

    connect(m_settings, &SettingsPanel::closed, this, [this] {
        m_tab = TabsBar::Phone; updateLayout(); m_dialer->focusDisplay();
    });
    connect(m_settings, &SettingsPanel::checkUpdate, this, [this] { runUpdateCheck(this, /*silent*/ false); });
    connect(m_settings, &SettingsPanel::saved, this, [this] { openSettings(); });

    connect(m_dialer, &DialerPanel::keyTone, this, [this](QChar d) {
        m_tones->playDtmf(d);
        sendDtmfIfInCall(d);
    });
    connect(m_dialer, &DialerPanel::callRequested, this, [this] { startOutgoingCall(); });

    connect(m_call, &CallPanel::hangupRequested, this, [this] { if (m_sip) m_sip->hangup(); });
    connect(m_call, &CallPanel::answerRequested, this, [this] { if (m_sip) m_sip->answer(); });
    connect(m_call, &CallPanel::rejectRequested, this, [this] { if (m_sip) m_sip->reject(); });
    connect(m_call, &CallPanel::muteClicked, this, [this] {
        if (!m_sip) return;
        const bool mute = !m_sip->isMuted();
        m_sip->setMute(mute);
        m_call->setMuteActive(mute);
    });
    connect(m_call, &CallPanel::holdClicked, this, [this] {
        if (!m_sip) return;
        const bool hold = !m_sip->isOnHold();
        m_sip->setHold(hold);
        m_call->setHoldActive(hold);
        m_call->setView(hold ? CallPanel::View::Held : CallPanel::View::Active);
    });
    connect(m_call, &CallPanel::transferClicked, this, [this] {
        if (!m_sip || m_sip->state() != LS::InCall) return;
        PromptDialog dlg(QStringLiteral("Transferir chamada"), QStringLiteral("Ramal de destino"),
                         QStringLiteral("Ex.: 1010"), false, true, QStringLiteral("Transferir"), this);
        if (dlg.exec() == QDialog::Accepted && !dlg.value().trimmed().isEmpty())
            m_sip->transfer(dlg.value());
    });
    // Cluster "Teclado": em chamada, mostra o discador para digitos DTMF; a aba
    // Telefone volta a vista da chamada.
    connect(m_call, &CallPanel::keypadClicked, this, [this] {
        m_dtmfPad = true;
        updateLayout();
        m_dialer->focusDisplay();
    });

    connect(m_recents, &RecentsPanel::redial, this, [this](const QString& num) {
        // Mostra ja no formato discavel (sem +55), espelhando o que sera ligado.
        const QString norm = SipManager::normalizeDestination(num);
        m_dialer->setNumber(norm.isEmpty() ? num : norm);
        m_tab = TabsBar::Phone;
        updateLayout();
        m_dialer->focusDisplay();
    });

    m_recents->setEntries(CallHistory::all());
    m_status->setRamal(m_config.username.trimmed());
}

// Clique nas abas. A TabsBar nao guarda estado: a pagina efetiva (e o destaque)
// saem sempre de updateLayout().
void MainWindow::onTabClicked(int idx) {
    if (m_windowLocked) return;
    if (idx == TabsBar::Settings) {
        m_settings->loadConfig();
        // Lista os dispositivos do motor ativo (vazio se ainda nao registrado).
        m_settings->setAudioDevices(m_sip ? m_sip->audioDevices() : QList<sphone::AudioDevice>());
    }
    if (idx == TabsBar::Phone) m_dtmfPad = false;   // volta a vista da chamada, se houver
    m_tab = idx;
    updateLayout();
    const LS st = m_sip ? m_sip->state() : LS::Offline;
    if (idx == TabsBar::Phone && st == LS::Idle) m_dialer->focusDisplay();
}

// =====================================================================
//  SIP / estado
// =====================================================================
void MainWindow::startSip() {
    m_sip = new SipManager(m_config, this);
    connect(m_sip, &SipManager::statusMessage, this, [this](const QString& m) { m_lastStatus = m; updateStatus(); });
    connect(m_sip, &SipManager::stateChanged, this, [this](LS s) { applyState(s); });
    connect(m_sip, &SipManager::incomingCallSignal, this, [this](const QString& num, const QString& name) {
        m_peerNumber = num; m_peerName = name;
    });
    connect(m_sip, &SipManager::registrationChanged, this, [this](bool ok) { m_registered = ok; updateStatus(); });
    connect(m_sip, &SipManager::callStarted, m_discord, &DiscordAudit::postStart);
    connect(m_sip, &SipManager::callEnded,   m_discord, &DiscordAudit::postEnd);
    connect(m_sip, &SipManager::callEnded, this, [this](const CallAudit& c) {
        CallHistory::add(c);
        m_recents->setEntries(CallHistory::all());
    });

    m_status->setRamal(m_config.username.trimmed());
    m_sip->start();
}

void MainWindow::applyState(LS st) {
    const LS prev = m_prevState;
    m_prevState = st;
    const bool wasCall = (prev == LS::Calling || prev == LS::InCall || prev == LS::Ringing);
    const bool nowCall = (st == LS::Calling || st == LS::InCall || st == LS::Ringing);
    if (wasCall && !nowCall) m_tones->playEnd();

    if (st != LS::Ringing) { stopRing(); stopAutoAnswer(); }
    if (st != LS::InCall) stopCallTimer();
    if (st != LS::Calling) m_tones->stopRingback();
    if (st != LS::Ringing && m_ringing) { m_ringing = false; setTopMost(false); }

    m_dialer->setCallEnabled(st == LS::Idle);

    switch (st) {
        case LS::Offline:
        case LS::Registering:
            m_call->setView(CallPanel::View::Idle);
            break;
        case LS::Idle:
            m_call->resetControls();
            m_call->setView(CallPanel::View::Idle);
            break;
        case LS::Ringing:
            m_call->setPeer(m_peerName, m_peerNumber);
            m_call->setView(CallPanel::View::Incoming);
            m_tab = TabsBar::Phone;
            m_dtmfPad = false;
            m_ringing = true;
            bringToForeground();
            setTopMost(true);
            startRing();
            if (m_config.autoAnswer) startAutoAnswer();
            break;
        case LS::Calling:
            m_call->setPeer(m_peerName, m_peerNumber);
            m_call->resetControls();
            m_call->setView(CallPanel::View::Outgoing);
            m_tab = TabsBar::Phone;
            m_dtmfPad = false;
            m_tones->startRingback();
            break;
        case LS::InCall:
            m_call->setPeer(m_peerName, m_peerNumber);
            m_call->setView(CallPanel::View::Active);
            startCallTimer();
            break;
    }
    updateLayout();
    updateStatus();
}

// Escolha da pagina do stack: chamada RECEBIDA forca a pagina de chamada e
// trava a janela; chamada de saida/ativa mostra a chamada na aba Telefone
// (com o discador acessivel via cluster "Teclado" p/ DTMF); sem chamada, a
// pagina segue a aba escolhida. A janela NUNCA muda de tamanho.
void MainWindow::updateLayout() {
    const LS st = m_sip ? m_sip->state() : LS::Offline;
    const bool incoming = (st == LS::Ringing);
    const bool inCall   = (st == LS::Calling || st == LS::InCall);
    if (!inCall) m_dtmfPad = false;

    setWindowLocked(incoming);
    if (m_tabsBar) m_tabsBar->setLocked(incoming);

    QWidget* page = nullptr;
    if (incoming)               page = m_call;
    else if (m_tab == TabsBar::Recents)  page = m_recents;
    else if (m_tab == TabsBar::Settings) page = m_settings;
    else                        page = (inCall && !m_dtmfPad) ? static_cast<QWidget*>(m_call)
                                                              : static_cast<QWidget*>(m_dialer);
    if (m_stack) m_stack->setCurrentWidget(page);
    if (m_tabsBar) m_tabsBar->setCurrent(incoming ? TabsBar::Phone : m_tab);
}

void MainWindow::setWindowLocked(bool on) {
    if (m_windowLocked == on) return;
    m_windowLocked = on;
    if (m_titleBar) m_titleBar->setLocked(on);
}

void MainWindow::startOutgoingCall() {
    if (!m_sip) return;
    const QString number = m_dialer->number();
    if (number.isEmpty() || m_sip->state() != LS::Idle) return;
    m_peerName.clear();
    m_peerNumber = number;
    m_sip->call(number);
}

void MainWindow::sendDtmfIfInCall(QChar key) {
    if (!m_sip || m_sip->state() != LS::InCall) return;
    quint8 digit;
    if (key == '*') digit = 10;
    else if (key == '#') digit = 11;
    else if (key >= '0' && key <= '9') digit = quint8(key.digitValue());
    else return;
    m_sip->sendDtmf(digit);
}

void MainWindow::updateStatus() {
    if (!m_status) return;
    const LS st = m_sip ? m_sip->state() : LS::Offline;
    const bool ok = m_registered
        && (st == LS::Idle || st == LS::Calling || st == LS::InCall || st == LS::Ringing);
    QString text;
    if (ok) {
        text = st == LS::InCall  ? QStringLiteral("Em chamada")
             : st == LS::Calling ? QStringLiteral("Chamando…")
             : st == LS::Ringing ? QStringLiteral("Recebendo chamada")
             : QStringLiteral("Disponível");
    } else {
        // Fora do ar, m_lastStatus carrega o motivo (config incompleta, falha
        // de registro, etc.) — mais util que um "Offline" generico.
        text = st == LS::Registering ? QStringLiteral("Conectando…") : m_lastStatus;
    }
    m_status->setStatus(ok, text);
    m_status->setRamal(m_config.username.trimmed());
}

// =====================================================================
//  Timer da chamada
// =====================================================================
void MainWindow::startCallTimer() {
    stopCallTimer();
    m_callStart = QDateTime::currentDateTime();
    m_call->setTimerText(QStringLiteral("00:00"));
    m_callTimer = new QTimer(this);
    m_callTimer->setInterval(500);
    connect(m_callTimer, &QTimer::timeout, this, [this] {
        qint64 sec = m_callStart.secsTo(QDateTime::currentDateTime());
        if (sec < 0) sec = 0;
        const QString t = sec >= 3600
            ? QStringLiteral("%1:%2:%3").arg(sec/3600).arg((sec%3600)/60, 2, 10, QChar('0')).arg(sec%60, 2, 10, QChar('0'))
            : QStringLiteral("%1:%2").arg(sec/60, 2, 10, QChar('0')).arg(sec%60, 2, 10, QChar('0'));
        m_call->setTimerText(t);
    });
    m_callTimer->start();
}
void MainWindow::stopCallTimer() {
    if (m_callTimer) { m_callTimer->stop(); m_callTimer->deleteLater(); m_callTimer = nullptr; }
}

// =====================================================================
//  Chamada recebida / janela
// =====================================================================
void MainWindow::startRing() {
    stopRing();
    m_ringtone->start();
    auto* t = new QTimer(this);
    t->setObjectName("ringTimer");
    t->setInterval(400);
    connect(t, &QTimer::timeout, this, [this] { keepRingingOnTop(); });
    t->start();
    flashWindow(true);
}
void MainWindow::stopRing() {
    if (auto* t = findChild<QTimer*>("ringTimer")) { t->stop(); t->deleteLater(); }
    if (m_ringtone) m_ringtone->stop();
    flashWindow(false);
}
// Auto-atendimento: 1s depois de comecar a tocar, atende sozinho e toca o chime
// de aviso — o atendente sabe que ja esta no ar sem ter clicado em nada. O timer
// e cancelado se a chamada sair do estado Ringing antes (atendida/perdida).
void MainWindow::startAutoAnswer() {
    stopAutoAnswer();
    m_autoAnswerTimer = new QTimer(this);
    m_autoAnswerTimer->setSingleShot(true);
    m_autoAnswerTimer->setInterval(1000);
    connect(m_autoAnswerTimer, &QTimer::timeout, this, [this] {
        if (!m_sip || m_sip->state() != LS::Ringing) return;
        m_tones->playAutoAnswer();
        m_sip->answer();
    });
    m_autoAnswerTimer->start();
}
void MainWindow::stopAutoAnswer() {
    if (m_autoAnswerTimer) { m_autoAnswerTimer->stop(); m_autoAnswerTimer->deleteLater(); m_autoAnswerTimer = nullptr; }
}

void MainWindow::bringToForeground() {
    showNormal(); raise(); activateWindow(); flashWindow(true);
}
void MainWindow::keepRingingOnTop() {
    if (!m_ringing) return;
    if (!isVisible()) showNormal();
    raise();
}
void MainWindow::setTopMost(bool on) {
#ifdef Q_OS_WIN
    HWND hwnd = reinterpret_cast<HWND>(winId());
    ::SetWindowPos(hwnd, on ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0,
                   SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
#else
    Q_UNUSED(on);
#endif
}
void MainWindow::flashWindow(bool start) {
#ifdef Q_OS_WIN
    FLASHWINFO fi{};
    fi.cbSize = sizeof(FLASHWINFO);
    fi.hwnd = reinterpret_cast<HWND>(winId());
    fi.dwFlags = start ? (FLASHW_ALL | FLASHW_TIMERNOFG) : FLASHW_STOP;
    fi.uCount = start ? UINT_MAX : 0;
    ::FlashWindowEx(&fi);
#else
    Q_UNUSED(start);
#endif
}

// =====================================================================
//  Bandeja / saida / config / janela
// =====================================================================
void MainWindow::buildTray() {
    m_tray = new QSystemTrayIcon(QIcon(":/assets/logo.ico"), this);
    m_tray->setToolTip(QString::fromUtf8("Soften Phone — clique para abrir"));
    auto* menu = new QMenu(this);
    menu->addAction(QStringLiteral("Abrir"), this, [this] { restoreFromTray(); });
    menu->addSeparator();
    menu->addAction(QStringLiteral("Sair"), this, [this] { tryExit(); });
    m_tray->setContextMenu(menu);
    connect(m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason r) {
        if (r == QSystemTrayIcon::Trigger || r == QSystemTrayIcon::DoubleClick) restoreFromTray();
    });
    connect(m_tray, &QSystemTrayIcon::messageClicked, this, [this] { restoreFromTray(); });
    m_tray->show();
}

void MainWindow::restoreFromTray() {
    setWindowState(windowState() & ~Qt::WindowMinimized);
    show(); showNormal(); raise(); activateWindow();
#ifdef Q_OS_WIN
    ::ShowWindow(reinterpret_cast<HWND>(winId()), SW_SHOW);
    ::SetForegroundWindow(reinterpret_cast<HWND>(winId()));
#endif
}

void MainWindow::hideToTray() {
    hide();
    if (!m_balloonShown && m_tray) {
        m_tray->showMessage(QStringLiteral("Soften Phone"),
                            QString::fromUtf8("Continua em execução na bandeja. Clique para abrir; use Sair para encerrar."),
                            QSystemTrayIcon::Information, 2500);
        m_balloonShown = true;
    }
}

void MainWindow::tryExit() {
    PromptDialog dlg(QStringLiteral("Encerrar Soften Phone"), QStringLiteral("Senha do supervisor"),
                     QString(), true, false, QStringLiteral("Encerrar"), this);
    if (dlg.exec() != QDialog::Accepted) return;
    QByteArray h = QCryptographicHash::hash(dlg.value().toUtf8(), QCryptographicHash::Sha256).toHex();
    if (QString::fromLatin1(h).toLower() == QString::fromLatin1(kSupervisorExitHash)) {
        m_reallyExit = true;
        ExitAudit::report(ExitAudit::ExitBySupervisor, m_config.username);
        if (m_tray) m_tray->hide();
        close();
        qApp->quit();
    } else {
        ExitAudit::report(ExitAudit::ExitAttemptDenied, m_config.username);
        QMessageBox::warning(this, QStringLiteral("Soften Phone"), QString::fromUtf8("Senha incorreta."));
    }
}

// Aplica o que o SettingsPanel gravou no config (sinal saved): salva, volta a
// aba Telefone, reinicia o registro SIP.
void MainWindow::openSettings() {
    m_config.save();
    m_lastStatus = QStringLiteral("Configurações salvas. Reiniciando registro…");

    m_tab = TabsBar::Phone;
    updateLayout();
    m_status->setRamal(m_config.username.trimmed());

    if (m_sip) { m_tones->stopRingback(); stopRing(); delete m_sip; m_sip = nullptr; m_registered = false; }
    if (m_config.isComplete()) startSip();
    updateStatus();
    m_dialer->focusDisplay();
}

void MainWindow::anchorBottomRight() {
    QScreen* scr = screen() ? screen() : QApplication::primaryScreen();
    if (!scr) return;
    const QRect a = scr->availableGeometry();   // exclui a barra de tarefas
    move(a.x() + a.width()  - width()  - dim::EdgeGap,
         a.y() + a.height() - height() - dim::EdgeGap);
}

// =====================================================================
//  Janela: cantos arredondados, fechar -> bandeja
// =====================================================================
void MainWindow::applyRoundedMask() {
#ifdef Q_OS_WIN
    // Windows 11: cantos arredondados nativos do DWM — antialiased, sem o
    // serrilhado da mascara 1-bit (QRegion). O DWM mantem reto quando maximizada.
    clearMask();
    const HWND hwnd = reinterpret_cast<HWND>(winId());
    // DWMWA_WINDOW_CORNER_PREFERENCE (33) = DWMWCP_ROUND (2). Constantes locais
    // para nao depender da versao do SDK.
    const DWORD kCornerPref = 33, kRound = 2, kBorderColor = 34;
    DWORD pref = kRound;
    ::DwmSetWindowAttribute(hwnd, kCornerPref, &pref, sizeof(pref));
    // Sem borda desenhada pelo DWM (DWMWA_COLOR_NONE).
    COLORREF noBorder = 0xFFFFFFFE;
    ::DwmSetWindowAttribute(hwnd, kBorderColor, &noBorder, sizeof(noBorder));
#else
    if (isMaximized()) { clearMask(); return; }
    QPainterPath path;
    path.addRoundedRect(QRectF(0, 0, width(), height()), dim::WindowRadius, dim::WindowRadius);
    setMask(QRegion(path.toFillPolygon().toPolygon()));
#endif
}

void MainWindow::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    applyRoundedMask();
}

void MainWindow::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    applyRoundedMask();
}

void MainWindow::closeEvent(QCloseEvent* e) {
    if (m_reallyExit) { e->accept(); return; }
    if (m_ringing) { bringToForeground(); e->ignore(); return; }
    hideToTray();
    e->ignore();
}

// Minimizar nunca e permitido (esconder = sumir da vista). Qualquer rota que
// minimize por fora — Win+M, Win+D ("Mostrar area de trabalho"), clique na barra
// de tarefas — cai aqui e e desfeita no proximo ciclo do loop de eventos.
void MainWindow::changeEvent(QEvent* e) {
    QWidget::changeEvent(e);
    if (e->type() == QEvent::WindowStateChange && !m_reallyExit
        && (windowState() & Qt::WindowMinimized)) {
        QTimer::singleShot(0, this, [this] {
            if (m_reallyExit) return;
            setWindowState(windowState() & ~Qt::WindowMinimized);
            showNormal(); raise(); activateWindow();
        });
    }
}

#ifdef Q_OS_WIN
bool MainWindow::nativeEvent(const QByteArray&, void* message, qintptr* result) {
    auto* msg = static_cast<MSG*>(message);
    if (msg->message == WM_SYSCOMMAND) {
        const WPARAM cmd = msg->wParam & 0xFFF0;
        // Janela fixa e anti-esconder: bloqueia minimizar, mover, maximizar e
        // redimensionar nativos (menu do sistema/Alt+Espaco, Win+setas, clique
        // na barra de tarefas). O tamanho e fixo e a posicao ancorada por codigo.
        if (cmd == SC_MINIMIZE || cmd == SC_MOVE || cmd == SC_MAXIMIZE || cmd == SC_SIZE) {
            if (result) *result = 0;
            return true;
        }
    }
    return false;
}
#endif

}  // namespace sphone
