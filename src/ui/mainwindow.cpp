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
#include <QHBoxLayout>
#include <QFrame>
#include <QSizeGrip>
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
#include <algorithm>

#ifdef Q_OS_WIN
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
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
    setWindowTitle(QStringLiteral("Soften Phone"));
    setWindowIcon(QIcon(":/assets/logo.ico"));
    setWindowFlag(Qt::FramelessWindowHint, true);
    setMinimumSize(dim::ShellMinW, dim::ShellMinH);
    resize(dim::ShellW, dim::ShellH);

    applyTheme(m_config.darkTheme);

    m_tones = new Tones(this);
    m_ringtone = new Ringtone(this);
    m_discord = new DiscordAudit(this);

    buildShell();
    buildTray();
    centerOnScreen();

    sphone::diag::log("MainWindow (shell desktop) criada.");

    QTimer::singleShot(0, this, [this] {
        if (!m_config.isComplete()) {
            m_lastStatus = QStringLiteral("Configuração incompleta");
            m_settingsOpen = true;       // abre o painel de Configuracoes encaixado
            m_settings->loadConfig();
            updateLayout();
            updatePill();
            return;                      // o registro inicia ao salvar (saved -> openSettings)
        }
        if (!m_sip) startSip();
    });
    QTimer::singleShot(4000, this, [this] { runUpdateCheck(this, /*silent*/ true); });
}

MainWindow::~MainWindow() {
    stopRing();
    stopCallTimer();
    stopMeter();
    stopStats();
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

    m_content = new QWidget(this);
    // Fundo da area de conteudo = superficie do painel, para que o espaco vazio
    // ao redor do discador (quando sozinho) nao apareca como "faixas pretas".
    m_content->setObjectName("ContentArea");
    m_content->setStyleSheet(QStringLiteral("#ContentArea{background:%1;}").arg(bodyBg().name()));
    auto* h = new QHBoxLayout(m_content);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(0);

    m_nav      = new NavRail(m_content);
    m_dialer   = new DialerPanel(m_content);
    m_call     = new CallPanel(m_content);
    m_recents  = new RecentsPanel(m_content);
    m_settings = new SettingsPanel(&m_config, m_content);
    m_dialer->setFixedWidth(dim::DialerW);   // coluna do discador, largura estavel

    m_sep = new QFrame(m_content);
    static_cast<QFrame*>(m_sep)->setFrameShape(QFrame::NoFrame);
    m_sep->setFixedWidth(1);
    m_sep->setStyleSheet(QStringLiteral("background:%1;").arg(border().name()));

    // [nav | discador | sep | (chamada|recentes|config) ]  — um painel extra por vez.
    h->addWidget(m_nav);
    h->addWidget(m_dialer, 0);
    h->addWidget(m_sep, 0);
    h->addWidget(m_call, 1);
    h->addWidget(m_recents, 1);
    h->addWidget(m_settings, 1);
    root->addWidget(m_content, 1);

    // Grip de redimensionamento (canto inferior direito).
    auto* grip = new QSizeGrip(this);
    grip->setFixedSize(16, 16);
    grip->raise();

    wirePanels();
    updateLayout();   // estado inicial: somente discador (centralizado)
    updatePill();
}

void MainWindow::wirePanels() {
    connect(m_titleBar, &TitleBar::minimizeClicked, this, [this] { showMinimized(); });
    connect(m_titleBar, &TitleBar::closeClicked,    this, [this] { close(); });

    connect(m_nav, &NavRail::goHome, this, [this] {
        if (m_windowLocked) return;
        m_recentsOpen = false; m_settingsOpen = false; updateLayout(); m_dialer->focusDisplay();
    });
    connect(m_nav, &NavRail::toggleRecents, this, [this] {
        if (m_windowLocked) return;
        m_recentsOpen = !m_recentsOpen; m_settingsOpen = false; updateLayout();
    });
    connect(m_nav, &NavRail::toggleSettings, this, [this] {
        if (m_windowLocked) return;
        m_settingsOpen = !m_settingsOpen; m_recentsOpen = false;
        if (m_settingsOpen) m_settings->loadConfig();
        updateLayout();
    });
    connect(m_nav, &NavRail::toggleTheme,  this, [this] {
        if (m_windowLocked) return;                 // nao reconstroi o shell durante o toque
        m_config.darkTheme = !m_config.darkTheme;
        m_config.save();
        applyTheme(m_config.darkTheme);
        rebuildShell();
    });
    m_nav->setRamal(m_config.username);

    connect(m_settings, &SettingsPanel::closed, this, [this] {
        m_settingsOpen = false; updateLayout();
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

    connect(m_recents, &RecentsPanel::redial, this, [this](const QString& num) {
        m_dialer->setNumber(num);
        m_dialer->focusDisplay();
    });

    m_recents->setEntries(CallHistory::all());
}

void MainWindow::rebuildShell() {
    // Remove o layout e os filhos para reconstruir com a paleta nova.
    if (auto* lay = layout()) {
        QLayoutItem* it;
        while ((it = lay->takeAt(0))) { if (it->widget()) it->widget()->deleteLater(); delete it; }
        delete lay;
    }
    m_titleBar = nullptr; m_nav = nullptr; m_dialer = nullptr;
    m_call = nullptr; m_recents = nullptr; m_settings = nullptr; m_content = nullptr;

    buildShell();
    applyState(m_sip ? m_sip->state() : LS::Offline);
}

// =====================================================================
//  SIP / estado
// =====================================================================
void MainWindow::startSip() {
    m_sip = new SipManager(m_config, this);
    connect(m_sip, &SipManager::statusMessage, this, [this](const QString& m) { m_lastStatus = m; updatePill(); });
    connect(m_sip, &SipManager::stateChanged, this, [this](LS s) { applyState(s); });
    connect(m_sip, &SipManager::incomingCallSignal, this, [this](const QString& num, const QString& name) {
        m_peerNumber = num; m_peerName = name;
    });
    connect(m_sip, &SipManager::registrationChanged, this, [this](bool ok) { m_registered = ok; updatePill(); });
    connect(m_sip, &SipManager::callStarted, m_discord, &DiscordAudit::postStart);
    connect(m_sip, &SipManager::callEnded,   m_discord, &DiscordAudit::postEnd);
    connect(m_sip, &SipManager::callEnded, this, [this](const CallAudit& c) {
        CallHistory::add(c);
        m_recents->setEntries(CallHistory::all());
    });

    m_nav->setRamal(m_config.username);
    m_sip->start();
}

void MainWindow::applyState(LS st) {
    const LS prev = m_prevState;
    m_prevState = st;
    const bool wasCall = (prev == LS::Calling || prev == LS::InCall || prev == LS::Ringing);
    const bool nowCall = (st == LS::Calling || st == LS::InCall || st == LS::Ringing);
    if (wasCall && !nowCall) m_tones->playEnd();

    if (st != LS::Ringing) stopRing();
    if (st != LS::InCall) { stopCallTimer(); stopMeter(); stopStats(); }
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
            m_ringing = true;
            bringToForeground();
            setTopMost(true);
            startRing();
            break;
        case LS::Calling:
            m_call->setPeer(m_peerName, m_peerNumber);
            m_call->resetControls();
            m_call->setView(CallPanel::View::Outgoing);
            m_tones->startRingback();
            break;
        case LS::InCall:
            m_call->setPeer(m_peerName, m_peerNumber);
            m_call->setView(CallPanel::View::Active);
            startCallTimer();
            startMeter();
            startStats();
            break;
    }
    updateLayout();
    updatePill();
}

// Visibilidade dos paineis: padrao = so discador; Recentes encaixa sob demanda;
// chamada de saida/ativa encaixa ao lado; chamada RECEBIDA toma a tela + trava.
void MainWindow::updateLayout() {
    const LS st = m_sip ? m_sip->state() : LS::Offline;
    const bool incoming   = (st == LS::Ringing);
    const bool callDocked = (st == LS::Calling || st == LS::InCall);
    const bool alone = !incoming && !callDocked && !m_recentsOpen && !m_settingsOpen;

    if (incoming) {
        m_dialer->hide(); m_recents->hide(); m_settings->hide(); m_sep->hide();
        m_call->show();
        setWindowLocked(true);
    } else {
        setWindowLocked(false);
        m_dialer->show();
        m_call->setVisible(callDocked);                   // um painel extra por vez
        m_recents->setVisible(!callDocked && m_recentsOpen);
        m_settings->setVisible(!callDocked && m_settingsOpen);
        m_sep->setVisible(!alone);
    }
    if (m_nav) {
        NavRail::Active a = NavRail::Active::Home;
        if (!incoming && !callDocked) {
            if (m_settingsOpen)      a = NavRail::Active::Settings;
            else if (m_recentsOpen)  a = NavRail::Active::Recents;
        }
        m_nav->setActive(a);
    }

    // Largura da janela: so o discador => estreita (colado a lateral, sem vazio);
    // com painel extra ou chamada => larga. Ao mudar de largura, recentraliza na
    // horizontal (mantendo Y) p/ que discador+painel fiquem centralizados.
    if (!isMaximized()) {
        const int aloneW = dim::RailW + dim::DialerW;
        const int targetW = alone ? aloneW : std::max(width(), dim::ShellW);
        setMinimumWidth(alone ? aloneW : dim::ShellMinW);
        if (width() != targetW) {
            resize(targetW, height());
            QScreen* scr = screen() ? screen() : QApplication::primaryScreen();
            if (scr) {
                const QRect a = scr->availableGeometry();
                move(a.x() + (a.width() - targetW) / 2, y());
            }
        }
    }
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

void MainWindow::updatePill() {
    if (!m_titleBar) return;
    const LS st = m_sip ? m_sip->state() : LS::Offline;
    bool ok = m_registered && (st == LS::Idle || st == LS::Calling || st == LS::InCall || st == LS::Ringing);
    QString text = ok ? QStringLiteral("REGISTRADO")
                 : st == LS::Registering ? QStringLiteral("CONECTANDO")
                 : st == LS::Offline ? QStringLiteral("OFFLINE")
                 : m_lastStatus.toUpper();
    m_titleBar->setRegistered(ok, text);
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

// Alimenta o waveform com o nivel de audio real (RX/TX combinado, peak-hold).
void MainWindow::startMeter() {
    stopMeter();
    m_levelShown = 0;
    m_meterTimer = new QTimer(this);
    m_meterTimer->setInterval(45);   // ~22 Hz -> janela de ~2.5s nos 56 bars
    connect(m_meterTimer, &QTimer::timeout, this, [this] {
        if (!m_sip) return;
        const float lvl = std::max(m_sip->micLevel(), m_sip->speakerLevel());
        m_levelShown = lvl > m_levelShown ? lvl : m_levelShown * 0.78f;   // peak-hold
        m_call->pushAudioLevel(m_levelShown);
    });
    m_meterTimer->start();
}
void MainWindow::stopMeter() {
    if (m_meterTimer) { m_meterTimer->stop(); m_meterTimer->deleteLater(); m_meterTimer = nullptr; }
}

// Telemetria do rodape de Recentes: codec / latencia / barras de sinal (~1 Hz).
void MainWindow::startStats() {
    stopStats();
    m_statsTimer = new QTimer(this);
    m_statsTimer->setInterval(1000);
    auto tick = [this] {
        if (!m_sip) return;
        const MediaStats s = m_sip->mediaStats();
        if (!s.valid) { m_recents->setTelemetry(QStringLiteral("—"), QStringLiteral("—"), QStringLiteral("▯▯▯▯")); return; }
        const QString codec = s.clockRate > 0
            ? QStringLiteral("%1 %2k").arg(s.codec.toUpper()).arg(s.clockRate / 1000)
            : s.codec.toUpper();
        const QString lat = s.rttMs >= 0 ? QStringLiteral("%1 ms").arg(s.rttMs) : QStringLiteral("—");
        int bars = s.lossPermil < 0 ? 3
                 : s.lossPermil <= 10 ? 4 : s.lossPermil <= 30 ? 3 : s.lossPermil <= 80 ? 2 : 1;
        QString sig;
        for (int i = 0; i < 4; ++i) sig += (i < bars ? QChar(0x25AE) : QChar(0x25AF));  // ▮ / ▯
        m_recents->setTelemetry(codec, lat, sig);
    };
    connect(m_statsTimer, &QTimer::timeout, this, tick);
    m_statsTimer->start();
    tick();
}
void MainWindow::stopStats() {
    if (m_statsTimer) { m_statsTimer->stop(); m_statsTimer->deleteLater(); m_statsTimer = nullptr; }
    if (m_recents) m_recents->clearTelemetry();
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

// Aplica o que o SettingsPanel gravou no config (sinal saved): salva, fecha o
// painel, troca o tema se mudou, reinicia o registro SIP.
void MainWindow::openSettings() {
    m_config.save();
    m_lastStatus = QStringLiteral("Configurações salvas. Reiniciando registro…");

    const bool themeChanged = (m_config.darkTheme != isDark());
    m_settingsOpen = false;
    if (themeChanged) {
        applyTheme(m_config.darkTheme);
        rebuildShell();            // recria o shell ja com o painel fechado
    } else {
        updateLayout();
    }
    if (m_nav) m_nav->setRamal(m_config.username);

    if (m_sip) { m_tones->stopRingback(); stopRing(); delete m_sip; m_sip = nullptr; m_registered = false; }
    if (m_config.isComplete()) startSip();
    updatePill();
}

void MainWindow::centerOnScreen() {
    QScreen* scr = screen() ? screen() : QApplication::primaryScreen();
    const QRect a = scr->availableGeometry();
    move(a.center() - QPoint(width() / 2, height() / 2));
}

// =====================================================================
//  Janela: mascara arredondada, fechar -> bandeja
// =====================================================================
void MainWindow::applyRoundedMask() {
    if (isMaximized()) { clearMask(); return; }
    QPainterPath path;
    path.addRoundedRect(QRectF(0, 0, width(), height()), dim::WindowRadius, dim::WindowRadius);
    setMask(QRegion(path.toFillPolygon().toPolygon()));
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

#ifdef Q_OS_WIN
bool MainWindow::nativeEvent(const QByteArray&, void* message, qintptr* result) {
    auto* msg = static_cast<MSG*>(message);
    // Chamada recebida trava a janela: bloqueia minimizar/mover/maximizar nativos.
    if (m_windowLocked && msg->message == WM_SYSCOMMAND) {
        const WPARAM cmd = msg->wParam & 0xFFF0;
        if (cmd == SC_MINIMIZE || cmd == SC_MOVE || cmd == SC_MAXIMIZE) {
            if (result) *result = 0;
            return true;
        }
    }
    return false;
}
#endif

}  // namespace sphone
