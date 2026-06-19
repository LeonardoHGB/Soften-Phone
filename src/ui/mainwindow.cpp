#include "ui/mainwindow.h"
#include "ui/controls.h"
#include "ui/promptdialog.h"
#include "ui/settingsform.h"
#include "ui/updateform.h"
#include "core/brand.h"
#include "core/version.h"
#include "core/diag.h"
#include "data/callhistory.h"
#include "data/discordaudit.h"
#include "audio/tones.h"
#include "audio/ringtone.h"

#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QScrollArea>
#include <QTimer>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QApplication>
#include <QScreen>
#include <QIcon>
#include <QImage>
#include <QMessageBox>
#include <QCryptographicHash>
#include <QRegularExpressionValidator>
#include <QCloseEvent>
#include <QKeyEvent>
#include <QDate>

#ifdef Q_OS_WIN
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#endif

using namespace brand;
using LS = sphone::SipManager::LineState;

// Segredos fora do versionamento: secret.h (gitignored) tem prioridade.
#if __has_include("data/secret.h")
#  include "data/secret.h"
#else
#  include "data/secret.example.h"
#endif

namespace {

// Hash SHA256 da senha de supervisor (vem de secret.h; a senha nao fica no codigo).
constexpr const char* kSupervisorExitHash = SPHONE_SUPERVISOR_HASH;

constexpr const char* kAppVersion = SPHONE_VERSION;

void setBg(QWidget* w, const QColor& c) {
    w->setAutoFillBackground(true);
    QPalette p = w->palette();
    p.setColor(QPalette::Window, c);
    w->setPalette(p);
}

QLabel* mkLabel(QWidget* parent, const QString& text, const QFont& font,
                const QColor& color, const QRect& geom, Qt::Alignment align) {
    auto* l = new QLabel(text, parent);
    l->setFont(font);
    l->setStyleSheet(QStringLiteral("color:%1;background:transparent;").arg(color.name()));
    l->setAlignment(align);
    if (!geom.isNull()) l->setGeometry(geom);
    l->setAttribute(Qt::WA_TransparentForMouseEvents);
    return l;
}

void setLabelColor(QLabel* l, const QColor& c) {
    l->setStyleSheet(QStringLiteral("color:%1;background:transparent;").arg(c.name()));
}

// Glifo MDL2 clicavel (engrenagem/historico/voltar) como botao plano.
QPushButton* mkGlyphButton(QWidget* parent, const QString& glyph, int px,
                           const QColor& color, const QRect& geom) {
    auto* b = new QPushButton(glyph, parent);
    b->setFont(iconPx(px));
    b->setGeometry(geom);
    b->setCursor(Qt::PointingHandCursor);
    b->setFlat(true);
    b->setStyleSheet(QStringLiteral(
        "QPushButton{border:none;background:transparent;color:%1;}").arg(color.name()));
    return b;
}

QString formatWhen(const QDateTime& when) {
    const QDate today = QDate::currentDate();
    if (when.date() == today) return when.toString("HH:mm");
    if (when.date() == today.addDays(-1)) return QStringLiteral("Ontem");
    return when.toString("dd/MM");
}

QString formatHms(int seconds) {
    if (seconds <= 0) return QString::fromUtf8("—");   // em-dash
    int h = seconds / 3600, m = (seconds % 3600) / 60, s = seconds % 60;
    return h > 0 ? QStringLiteral("%1:%2:%3").arg(h).arg(m, 2, 10, QChar('0')).arg(s, 2, 10, QChar('0'))
                 : QStringLiteral("%1:%2").arg(m, 2, 10, QChar('0')).arg(s, 2, 10, QChar('0'));
}

}  // namespace

namespace sphone {

MainWindow::MainWindow(QWidget* parent) : QWidget(parent) {
    m_config = SipConfig::load();
    setWindowTitle(QStringLiteral("Soften Phone"));
    setWindowIcon(QIcon(":/assets/logo.ico"));
    setFixedSize(dim::WinW, dim::WinH);

    applyTheme(m_config.darkTheme);   // tema antes de montar a UI
    setBg(this, bodyBg());

    m_tones = new Tones(this);         // antes de buildViews (o teclado usa o DTMF)
    m_ringtone = new Ringtone(this);
    m_discord = new DiscordAudit(this);

    buildViews();
    buildTray();
    lockToCorner();

    m_posGuard = new QTimer(this);
    m_posGuard->setInterval(1000);
    connect(m_posGuard, &QTimer::timeout, this, [this] { lockToCorner(); });
    m_posGuard->start();

    sphone::diag::log("MainWindow criada.");

    // Apos a janela existir: 1a configuracao se incompleta, depois registra.
    QTimer::singleShot(0, this, [this] {
        if (!m_config.isComplete()) {
            setStatus(QStringLiteral("Configuracao incompleta. Abra Configuracoes."));
            setStatusLevel(StatusLevel::Error);
            openSettings();             // openSettings ja reinicia o SIP se completou
            if (!m_config.isComplete()) return;
        }
        if (!m_sip) startSip();
    });

    // Checagem de atualizacao silenciosa, alguns segundos apos subir (nao bloqueia o boot).
    QTimer::singleShot(4000, this, [this] { runUpdateCheck(this, /*silent*/ true); });
}

MainWindow::~MainWindow() {
    stopRing();
    stopCallTimer();
    stopMeter();
    if (m_sip) delete m_sip;
}

// =====================================================================
//  Construcao das views
// =====================================================================

void MainWindow::buildViews() {
    m_dialerView   = buildDialerView();
    m_incomingView = buildIncomingView();
    m_inCallView   = buildInCallView();
    m_historyView  = buildHistoryView();
    for (QWidget* v : { m_dialerView, m_incomingView, m_inCallView, m_historyView }) {
        v->setGeometry(0, 0, dim::WinW, dim::WinH);
        v->hide();
    }
    showView(m_dialerView);
}

QWidget* MainWindow::buildDialerView() {
    auto* root = new QWidget(this);
    setBg(root, bodyBg());

    auto* header = new QWidget(root);
    header->setGeometry(0, 0, 360, 58);
    setBg(header, Navy);

    auto* logo = new QLabel(header);
    logo->setGeometry(16, 15, 28, 28);
    logo->setAttribute(Qt::WA_TransparentForMouseEvents);
    const QPixmap lp = whiteLogo();
    if (!lp.isNull())
        logo->setPixmap(lp.scaled(28, 28, Qt::KeepAspectRatio, Qt::SmoothTransformation));

    mkLabel(header, QStringLiteral("Soften Phone"), uiPt(13.5), Qt::white,
            QRect(52, 0, 170, 58), Qt::AlignVCenter | Qt::AlignLeft);

    auto* gear = mkGlyphButton(header, glyph::Settings, 15, PaleBlueText, QRect(322, 19, 22, 22));
    connect(gear, &QPushButton::clicked, this, [this] { openSettings(); });
    auto* hist = mkGlyphButton(header, glyph::History, 15, PaleBlueText, QRect(294, 19, 22, 22));
    connect(hist, &QPushButton::clicked, this, [this] { showHistory(); });

    m_stateDot   = mkLabel(header, QStringLiteral("●"), uiPt(8), QColor(Qt::gray),
                           QRect(226, 20, 12, 18), Qt::AlignCenter);
    m_ramalLabel = mkLabel(header, m_config.username, uiPt(9.5), PaleBlueText,
                           QRect(240, 20, 48, 18), Qt::AlignVCenter | Qt::AlignLeft);

    auto* body = new QWidget(root);
    body->setGeometry(0, 58, 360, 560 - 58);
    setBg(body, bodyBg());
    auto* vb = new QVBoxLayout(body);
    vb->setContentsMargins(16, 14, 16, 10);
    vb->setSpacing(0);

    m_displayCard = new RoundedCard();
    m_displayCard->fillColor = panelGray();
    m_displayCard->borderColor = border();
    m_displayCard->borderThickness = 1;
    m_displayCard->radius = 10;
    m_displayCard->setFixedHeight(44);
    auto* cardLay = new QVBoxLayout(m_displayCard);
    cardLay->setContentsMargins(14, 0, 14, 0);
    m_display = new QLineEdit(m_displayCard);
    m_display->setFont(uiPt(20));
    m_display->setAlignment(Qt::AlignCenter);
    m_display->setPlaceholderText(QString::fromUtf8("Digite o número"));
    m_display->setStyleSheet(QStringLiteral(
        "QLineEdit{background:transparent;border:none;color:%1;}").arg(textPrimary().name()));
    m_display->setValidator(new QRegularExpressionValidator(
        QRegularExpression(QStringLiteral("[0-9*#+]*")), m_display));
    connect(m_display, &QLineEdit::returnPressed, this, [this] { startOutgoingCall(); });
    m_display->installEventFilter(this);   // toca o tom DTMF ao digitar no teclado fisico
    cardLay->addStretch();
    cardLay->addWidget(m_display);
    cardLay->addStretch();
    vb->addWidget(m_displayCard);
    vb->addSpacing(12);

    vb->addWidget(buildDialpad(), 1);
    vb->addSpacing(12);

    m_ligarBtn = new ActionButton();
    m_ligarBtn->setText(QStringLiteral("Ligar"));
    m_ligarBtn->glyph = glyph::Phone;
    m_ligarBtn->fill = Cyan;
    m_ligarBtn->radius = 12;
    m_ligarBtn->setFixedHeight(58);
    connect(m_ligarBtn, &ActionButton::clicked, this, [this] { startOutgoingCall(); });
    vb->addWidget(m_ligarBtn);
    vb->addSpacing(8);

    m_statusIndicator = new StatusIndicator();
    m_statusIndicator->setFont(uiPt(9));
    m_statusIndicator->foreground = textSecondary();
    m_statusIndicator->setText(m_lastStatus);
    m_statusIndicator->setLevel(m_lastLevel);
    m_statusIndicator->setFixedHeight(22);
    vb->addWidget(m_statusIndicator);

    auto* ver = new QLabel(QStringLiteral("v%1").arg(kAppVersion));
    ver->setFont(uiPt(8));
    ver->setStyleSheet(QStringLiteral("color:%1;background:transparent;").arg(textTertiary().name()));
    ver->setAlignment(Qt::AlignCenter);
    ver->setFixedHeight(16);
    vb->addWidget(ver);

    return root;
}

QWidget* MainWindow::buildDialpad() {
    auto* w = new QWidget();
    auto* g = new QGridLayout(w);
    g->setContentsMargins(0, 0, 0, 0);
    g->setSpacing(8);

    struct K { const char* k; const char* l; };
    static const K keys[12] = {
        {"1", ""}, {"2", "ABC"}, {"3", "DEF"},
        {"4", "GHI"}, {"5", "JKL"}, {"6", "MNO"},
        {"7", "PQRS"}, {"8", "TUV"}, {"9", "WXYZ"},
        {"*", ""}, {"0", "+"}, {"#", ""}
    };
    for (int i = 0; i < 12; ++i) {
        auto* key = new DialKey();
        key->keyChar = QString::fromUtf8(keys[i].k);
        key->letters = QString::fromUtf8(keys[i].l);
        key->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        const QString kc = key->keyChar;
        connect(key, &DialKey::clicked, this, [this, kc] {
            if (!kc.isEmpty()) m_tones->playDtmf(kc[0]);   // tom DTMF da tecla
            appendToDisplay(kc);
        });
        g->addWidget(key, i / 3, i % 3);
    }
    return w;
}

QWidget* MainWindow::buildIncomingView() {
    auto* root = new QWidget(this);
    setBg(root, Navy);

    mkLabel(root, QStringLiteral("CHAMADA RECEBIDA"), uiPt(9.5), LightBlueText,
            QRect(0, 40, 360, 18), Qt::AlignCenter);

    auto* avatar = new Avatar(root);
    avatar->glyphSize = 42;
    avatar->setGeometry((360 - 88) / 2, 84, 88, 88);

    m_incName   = mkLabel(root, QString(), uiPt(15), Qt::white, QRect(0, 188, 360, 26), Qt::AlignCenter);
    m_incNumber = mkLabel(root, QString(), uiPt(11), LightBlueText, QRect(0, 216, 360, 20), Qt::AlignCenter);
    mkLabel(root, QString::fromUtf8("tocando…"), uiPt(9.5), DimBlueText,
            QRect(0, 238, 360, 18), Qt::AlignCenter);

    auto* reject = new IconButton(root);
    reject->glyph = glyph::Phone; reject->glyphRotation = 135; reject->glyphSize = 24;
    reject->fill = Red; reject->setGeometry(100, 430, 60, 60);
    connect(reject, &IconButton::clicked, this, [this] { if (m_sip) m_sip->reject(); });

    auto* answer = new IconButton(root);
    answer->glyph = glyph::Phone; answer->glyphSize = 24;
    answer->fill = Green; answer->setGeometry(200, 430, 60, 60);
    connect(answer, &IconButton::clicked, this, [this] { if (m_sip) m_sip->answer(); });

    mkLabel(root, QStringLiteral("Recusar"), uiPt(9), PaleBlueText, QRect(90, 498, 80, 16), Qt::AlignCenter);
    mkLabel(root, QStringLiteral("Atender"), uiPt(9), PaleBlueText, QRect(190, 498, 80, 16), Qt::AlignCenter);

    return root;
}

QWidget* MainWindow::buildInCallView() {
    auto* root = new QWidget(this);
    setBg(root, bodyBg());

    auto* top = new QWidget(root);
    top->setGeometry(0, 0, 360, 160);
    setBg(top, Navy);

    auto* avatar = new Avatar(top);
    avatar->glyphSize = 30;
    avatar->setGeometry((360 - 64) / 2, 22, 64, 64);

    m_callName      = mkLabel(top, QString(), uiPt(13.5), Qt::white, QRect(0, 92, 360, 24), Qt::AlignCenter);
    m_callTimerLabel = mkLabel(top, QStringLiteral("00:00"), uiPt(17), LightBlueText, QRect(0, 118, 360, 28), Qt::AlignCenter);

    // Controles (Mudo / Espera / Transferir).
    auto* controls = new QWidget(root);
    controls->setGeometry(16, 178, 328, 100);
    setBg(controls, bodyBg());
    auto* hb = new QHBoxLayout(controls);
    hb->setContentsMargins(0, 0, 0, 0);
    hb->setSpacing(8);

    m_muteCtrl = new CallControl();
    m_muteCtrl->glyph = glyph::Microphone; m_muteCtrl->label = QStringLiteral("Mudo");
    connect(m_muteCtrl, &CallControl::clicked, this, [this] {
        if (!m_sip) return;
        bool mute = !m_sip->isMuted();
        m_sip->setMute(mute);
        m_muteCtrl->setActive(mute);
        m_muteCtrl->glyph = mute ? glyph::Mute : glyph::Microphone;
        m_muteCtrl->update();
    });
    m_holdCtrl = new CallControl();
    m_holdCtrl->glyph = glyph::Pause; m_holdCtrl->label = QStringLiteral("Espera");
    connect(m_holdCtrl, &CallControl::clicked, this, [this] {
        if (!m_sip) return;
        bool hold = !m_sip->isOnHold();
        m_sip->setHold(hold);
        m_holdCtrl->setActive(hold);
    });
    auto* transfer = new CallControl();
    transfer->glyph = glyph::Transfer; transfer->label = QStringLiteral("Transferir");
    connect(transfer, &CallControl::clicked, this, [this] {
        if (!m_sip || m_sip->state() != LS::InCall) return;
        PromptDialog dlg(QStringLiteral("Transferir chamada"), QStringLiteral("Ramal de destino"),
                         QStringLiteral("Ex.: 1010"), false, true, QStringLiteral("Transferir"), this);
        if (dlg.exec() == QDialog::Accepted && !dlg.value().trimmed().isEmpty())
            m_sip->transfer(dlg.value());
    });
    hb->addWidget(m_muteCtrl);
    hb->addWidget(m_holdCtrl);
    hb->addWidget(transfer);

    // Medidores (acima do Encerrar) e botao Encerrar (no rodape).
    m_speakerBar = new LevelBar(root);
    m_speakerBar->caption = QStringLiteral("Alto-falante"); m_speakerBar->glyph = glyph::Volume;
    m_speakerBar->setGeometry(16, 432, 328, 26);
    m_micBar = new LevelBar(root);
    m_micBar->caption = QStringLiteral("Microfone"); m_micBar->glyph = glyph::Microphone;
    m_micBar->setGeometry(16, 458, 328, 26);

    auto* encerrar = new ActionButton(root);
    encerrar->setText(QStringLiteral("Encerrar"));
    encerrar->glyph = glyph::Phone; encerrar->glyphRotation = 135;
    encerrar->fill = Red; encerrar->radius = 12;
    encerrar->setGeometry(16, 496, 328, 52);
    connect(encerrar, &ActionButton::clicked, this, [this] { if (m_sip) m_sip->hangup(); });

    return root;
}

QWidget* MainWindow::buildHistoryView() {
    auto* root = new QWidget(this);
    setBg(root, bodyBg());

    auto* header = new QWidget(root);
    header->setGeometry(0, 0, 360, 58);
    setBg(header, Navy);

    auto* back = mkGlyphButton(header, glyph::Back, 15, PaleBlueText, QRect(12, 18, 24, 22));
    connect(back, &QPushButton::clicked, this, [this] { showView(m_dialerView); });
    mkLabel(header, QStringLiteral("Recentes"), uiPt(13.5), Qt::white, QRect(46, 0, 200, 58),
            Qt::AlignVCenter | Qt::AlignLeft);

    // Estatisticas do dia.
    auto* stats = new QWidget(root);
    stats->setGeometry(0, 58, 360, 64);
    setBg(stats, bodyBg());
    auto* sh = new QHBoxLayout(stats);
    sh->setContentsMargins(6, 8, 6, 6);
    sh->setSpacing(0);
    auto makeStat = [&](const QString& cap, const QColor& col, QLabel*& outNum) {
        auto* cell = new QWidget();
        auto* cv = new QVBoxLayout(cell);
        cv->setContentsMargins(0, 0, 0, 0); cv->setSpacing(0);
        outNum = new QLabel(QStringLiteral("0"));
        outNum->setFont(semiPt(14));
        outNum->setStyleSheet(QStringLiteral("color:%1;background:transparent;").arg(col.name()));
        outNum->setAlignment(Qt::AlignCenter);
        outNum->setFixedHeight(26);
        auto* c = new QLabel(cap);
        c->setFont(uiPt(8));
        c->setStyleSheet(QStringLiteral("color:%1;background:transparent;").arg(textSecondary().name()));
        c->setAlignment(Qt::AlignCenter);
        c->setFixedHeight(16);
        cv->addWidget(outNum); cv->addWidget(c); cv->addStretch();
        sh->addWidget(cell);
    };
    makeStat(QStringLiteral("Recebidas"), Green, m_statIn);
    makeStat(QStringLiteral("Efetuadas"), Cyan, m_statOut);
    makeStat(QStringLiteral("Perdidas"), Red, m_statMissed);
    makeStat(QStringLiteral("Em chamada"), Navy, m_statTime);

    // Lista rolavel.
    auto* scroll = new QScrollArea(root);
    scroll->setGeometry(0, 122, 360, 560 - 122);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setBg(scroll->viewport(), bodyBg());
    auto* listContainer = new QWidget();
    setBg(listContainer, bodyBg());
    m_historyListLayout = new QVBoxLayout(listContainer);
    m_historyListLayout->setContentsMargins(0, 2, 0, 8);
    m_historyListLayout->setSpacing(0);
    m_historyListLayout->addStretch();
    scroll->setWidget(listContainer);

    return root;
}

void MainWindow::showView(QWidget* v) {
    if (!v) return;
    v->show();
    v->raise();
    for (QWidget* o : { m_dialerView, m_incomingView, m_inCallView, m_historyView })
        if (o && o != v) o->hide();
    if (v == m_dialerView && m_display) m_display->setFocus();
}

// =====================================================================
//  Historico
// =====================================================================

void MainWindow::showHistory() {
    refreshHistory();
    showView(m_historyView);
}

void MainWindow::refreshHistory() {
    const DayStats s = CallHistory::today();
    m_statIn->setText(QString::number(s.incoming));
    m_statOut->setText(QString::number(s.outgoing));
    m_statMissed->setText(QString::number(s.missed));
    m_statTime->setText(formatHms(s.talkSeconds));

    // Limpa a lista (mantem o stretch final).
    while (m_historyListLayout->count() > 1) {
        QLayoutItem* it = m_historyListLayout->takeAt(0);
        if (it->widget()) it->widget()->deleteLater();
        delete it;
    }

    const QList<CallAudit> all = CallHistory::all();
    if (all.isEmpty()) {
        auto* empty = new QLabel(QStringLiteral("Nenhuma chamada registrada ainda."));
        empty->setFont(uiPt(9.5));
        empty->setStyleSheet(QStringLiteral("color:%1;background:transparent;").arg(textTertiary().name()));
        empty->setAlignment(Qt::AlignCenter);
        empty->setFixedHeight(60);
        m_historyListLayout->insertWidget(0, empty);
    } else {
        int i = 0;
        for (const CallAudit& c : all)
            m_historyListLayout->insertWidget(i++, buildHistoryRow(c));
    }
}

QWidget* MainWindow::buildHistoryRow(const CallAudit& c) {
    const bool inbound = c.direction == CallDirection::Inbound;
    QColor accent = c.answeredElsewhere ? textTertiary()
                  : c.answered ? (inbound ? Green : Cyan)
                  : (inbound ? Red : textTertiary());

    auto* row = new ClickableWidget();
    row->setFixedSize(322, 54);
    setBg(row, bodyBg());
    connect(row, &ClickableWidget::clicked, this, [this, num = c.peerNumber] { redialFromHistory(num); });

    // Divisoria inferior.
    auto* line = new QWidget(row);
    line->setGeometry(12, 53, 322 - 24, 1);
    setBg(line, border());

    mkLabel(row, glyph::Phone, iconPx(15), accent, QRect(8, 0, 32, 54), Qt::AlignCenter);

    QString titleText = !c.peerName.trimmed().isEmpty() ? c.peerName
                       : (c.peerNumber.trimmed().isEmpty() ? QStringLiteral("desconhecido") : c.peerNumber);
    mkLabel(row, titleText, uiPt(11), textPrimary(), QRect(46, 7, 180, 22), Qt::AlignVCenter | Qt::AlignLeft);

    const QString dir = inbound ? QStringLiteral("Recebida") : QStringLiteral("Efetuada");
    QString sub = (!c.peerName.trimmed().isEmpty() && !c.peerNumber.trimmed().isEmpty())
                  ? QStringLiteral("%1 · %2").arg(c.peerNumber, c.outcome)
                  : QStringLiteral("%1 · %2").arg(dir, c.outcome);
    mkLabel(row, sub, uiPt(8.5), textSecondary(), QRect(46, 28, 190, 18), Qt::AlignVCenter | Qt::AlignLeft);

    mkLabel(row, formatWhen(c.startedLocal), uiPt(9), textTertiary(), QRect(236, 7, 78, 22),
            Qt::AlignVCenter | Qt::AlignRight);
    if (c.answered && c.durationSeconds > 0)
        mkLabel(row, formatHms(c.durationSeconds), uiPt(8.5), textTertiary(), QRect(236, 28, 78, 18),
                Qt::AlignVCenter | Qt::AlignRight);

    return row;
}

void MainWindow::redialFromHistory(const QString& number) {
    if (number.trimmed().isEmpty() || number == QStringLiteral("desconhecido")) return;
    showView(m_dialerView);
    m_display->setText(number);
    m_display->setCursorPosition(number.length());
    m_display->setFocus();
}

// =====================================================================
//  SIP / estado
// =====================================================================

void MainWindow::startSip() {
    m_sip = new SipManager(m_config, this);
    connect(m_sip, &SipManager::statusMessage, this, [this](const QString& m) { setStatus(m); });
    connect(m_sip, &SipManager::stateChanged, this, [this](SipManager::LineState s) { applyState(s); });
    connect(m_sip, &SipManager::incomingCallSignal, this, [this](const QString& num, const QString& name) {
        m_peerNumber = num; m_peerName = name;
    });
    connect(m_sip, &SipManager::registrationChanged, this, [this](bool ok) {
        setLabelColor(m_stateDot, ok ? Green : QColor(Qt::gray));
    });
    // Historico local + auditoria no webhook do Discord (mensagem ao comecar, editada ao encerrar).
    connect(m_sip, &SipManager::callStarted, m_discord, &DiscordAudit::postStart);
    connect(m_sip, &SipManager::callEnded,   m_discord, &DiscordAudit::postEnd);
    connect(m_sip, &SipManager::callEnded, this, [this](const CallAudit& c) {
        CallHistory::add(c);
        if (m_historyView && m_historyView->isVisible()) refreshHistory();
    });

    m_ramalLabel->setText(m_config.username);
    m_sip->start();
}

void MainWindow::applyState(SipManager::LineState st) {
    // Efeitos sonoros de transicao: bipe ao INICIAR a chamada de saida (entra em
    // Calling) e bipe ao ENCERRAR (sai de qualquer estado de chamada para Idle/Offline).
    const LS prev = m_prevState;
    m_prevState = st;
    const bool wasCall = (prev == LS::Calling || prev == LS::InCall || prev == LS::Ringing);
    const bool nowCall = (st == LS::Calling || st == LS::InCall || st == LS::Ringing);
    // "Chamando" = ringback continuo (abaixo, no case Calling); bipe ao ENCERRAR.
    if (wasCall && !nowCall) m_tones->playEnd();

    if (st != LS::Ringing) stopRing();
    if (st != LS::InCall) { stopCallTimer(); stopMeter(); }
    if (st != LS::Calling) m_tones->stopRingback();   // ringback so enquanto disca

    setStatusLevel(st == LS::Offline ? StatusLevel::Error
                 : st == LS::Registering ? StatusLevel::Warn : StatusLevel::Ok);

    if (st != LS::Ringing && m_ringing) { m_ringing = false; setTopMost(false); }

    switch (st) {
        case LS::Offline:
            setLabelColor(m_stateDot, QColor(Qt::gray));
            m_ligarBtn->setEnabled(false);
            showView(m_dialerView);
            break;
        case LS::Registering:
            setLabelColor(m_stateDot, QColor(0xFF, 0xA5, 0x00));   // Orange
            m_ligarBtn->setEnabled(false);
            showView(m_dialerView);
            break;
        case LS::Idle:
            setLabelColor(m_stateDot, Green);
            m_ligarBtn->setEnabled(true);
            resetInCallControls();
            showView(m_dialerView);
            break;
        case LS::Ringing: {
            bool hasName = !m_peerName.isEmpty();
            m_incName->setText(hasName ? m_peerName
                              : (m_peerNumber.isEmpty() ? QStringLiteral("Chamada recebida") : m_peerNumber));
            m_incNumber->setText(hasName ? m_peerNumber : QString());
            showView(m_incomingView);
            m_ringing = true;
            bringToForeground();
            setTopMost(true);
            startRing();
            break;
        }
        case LS::Calling:
            m_callName->setText(m_peerName.isEmpty() ? m_peerNumber : m_peerName);
            m_callTimerLabel->setText(QString::fromUtf8("Chamando…"));
            resetInCallControls();
            showView(m_inCallView);
            m_tones->startRingback();   // som de "chamando" enquanto a outra ponta toca
            break;
        case LS::InCall:
            m_callName->setText(m_peerName.isEmpty() ? m_peerNumber : m_peerName);
            showView(m_inCallView);
            startCallTimer();
            startMeter();
            break;
    }
}

void MainWindow::resetInCallControls() {
    if (!m_muteCtrl) return;
    m_muteCtrl->setActive(false);
    m_muteCtrl->glyph = glyph::Microphone;
    m_muteCtrl->update();
    m_holdCtrl->setActive(false);
}

void MainWindow::setStatus(const QString& msg) {
    m_lastStatus = msg;
    if (m_statusIndicator) m_statusIndicator->setText(msg);
}

void MainWindow::setStatusLevel(StatusLevel lvl) {
    m_lastLevel = lvl;
    if (m_statusIndicator) m_statusIndicator->setLevel(lvl);
}

void MainWindow::startOutgoingCall() {
    if (!m_sip) return;
    QString number = m_display->text().trimmed();
    if (number.isEmpty()) return;
    if (m_sip->state() != LS::Idle) return;
    m_peerName.clear();
    m_peerNumber = number;
    m_sip->call(number);
}

void MainWindow::appendToDisplay(const QString& key) {
    int pos = m_display->cursorPosition();
    QString t = m_display->text();
    t.insert(pos, key);
    m_display->setText(t);
    m_display->setCursorPosition(pos + key.length());
    // tom DTMF local -> fase 7
    sendDtmfIfInCall(key);
}

void MainWindow::sendDtmfIfInCall(const QString& key) {
    if (!m_sip || m_sip->state() != LS::InCall) return;
    quint8 digit;
    if (key == "*") digit = 10;
    else if (key == "#") digit = 11;
    else {
        bool ok = false; int d = key.toInt(&ok);
        if (!ok) return;
        digit = quint8(d);
    }
    m_sip->sendDtmf(digit);
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
    // Toca o tom DTMF quando o usuario digita um caractere valido no campo (o
    // QValidator ja barra invalidos). Nao consome o evento.
    if (watched == m_display && event->type() == QEvent::KeyPress) {
        const QString txt = static_cast<QKeyEvent*>(event)->text();
        if (txt.size() == 1) {
            const QChar c = txt.at(0);
            if ((c >= '0' && c <= '9') || c == '*' || c == '#' || c == '+')
                m_tones->playDtmf(c);
        }
    }
    return QWidget::eventFilter(watched, event);
}

// =====================================================================
//  Timers
// =====================================================================

void MainWindow::startCallTimer() {
    stopCallTimer();
    m_callStart = QDateTime::currentDateTime();
    m_callTimerLabel->setText(QStringLiteral("00:00"));
    m_callTimer = new QTimer(this);
    m_callTimer->setInterval(500);
    connect(m_callTimer, &QTimer::timeout, this, [this] {
        qint64 sec = m_callStart.secsTo(QDateTime::currentDateTime());
        if (sec < 0) sec = 0;
        m_callTimerLabel->setText(QStringLiteral("%1:%2")
            .arg(sec / 60, 2, 10, QChar('0')).arg(sec % 60, 2, 10, QChar('0')));
    });
    m_callTimer->start();
}

void MainWindow::stopCallTimer() {
    if (m_callTimer) { m_callTimer->stop(); m_callTimer->deleteLater(); m_callTimer = nullptr; }
}

void MainWindow::startMeter() {
    stopMeter();
    m_micShown = 0; m_spkShown = 0;
    m_meterTimer = new QTimer(this);
    m_meterTimer->setInterval(60);
    connect(m_meterTimer, &QTimer::timeout, this, [this] {
        if (!m_sip) return;
        float m = m_sip->micLevel();
        float sp = m_sip->speakerLevel();
        m_micShown = m > m_micShown ? m : m_micShown * 0.82f;   // peak-hold
        m_spkShown = sp > m_spkShown ? sp : m_spkShown * 0.82f;
        m_micBar->setLevel(m_micShown);
        m_speakerBar->setLevel(m_spkShown);
    });
    m_meterTimer->start();
}

void MainWindow::stopMeter() {
    if (m_meterTimer) { m_meterTimer->stop(); m_meterTimer->deleteLater(); m_meterTimer = nullptr; }
    if (m_micBar) m_micBar->setLevel(0);
    if (m_speakerBar) m_speakerBar->setLevel(0);
}

// =====================================================================
//  Chamada recebida / janela
// =====================================================================

void MainWindow::startRing() {
    stopRing();
    m_ringtone->start();   // chamando.mp3 em loop (Qt Multimedia)
    // Mantem a janela na frente enquanto toca + realce na barra de tarefas.
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
    showNormal();
    raise();
    activateWindow();
    flashWindow(true);
}

void MainWindow::keepRingingOnTop() {
    if (!m_ringing) return;
    if (!isVisible()) showNormal();
    raise();
}

void MainWindow::setTopMost(bool on) {
    // IMPORTANTE: NAO usar setWindowFlag(WindowStaysOnTopHint) — no Qt/Windows
    // trocar flags ESCONDE e recria a janela nativa, corrompendo o estado (a
    // janela "some" e o show() nao recupera). SetWindowPos so muda o Z-order.
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
    fi.dwTimeout = 0;
    ::FlashWindowEx(&fi);
#else
    Q_UNUSED(start);
#endif
}

// =====================================================================
//  Bandeja / saida / config
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
    // A prova de falha: limpa minimizacao, mostra, traz pra frente e reposiciona.
    setWindowState(windowState() & ~Qt::WindowMinimized);
    show();
    showNormal();
    raise();
    activateWindow();
#ifdef Q_OS_WIN
    // Garante que o Windows realmente exiba (caso o estado tenha ficado dessincronizado).
    ::ShowWindow(reinterpret_cast<HWND>(winId()), SW_SHOW);
    ::SetForegroundWindow(reinterpret_cast<HWND>(winId()));
#endif
    lockToCorner();
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
        if (m_tray) m_tray->hide();   // remove o icone da bandeja antes de sair
        close();
        // quitOnLastWindowClosed=false (fechar vai p/ bandeja): para ENCERRAR de
        // fato o processo e preciso encerrar o event loop explicitamente.
        qApp->quit();
    } else {
        QMessageBox::warning(this, QStringLiteral("Soften Phone"), QString::fromUtf8("Senha incorreta."));
    }
}

void MainWindow::openSettings() {
    const bool wasDark = m_config.darkTheme;
    SettingsForm dlg(&m_config, this);
    if (dlg.exec() != QDialog::Accepted) return;

    m_config.save();
    setStatus(QStringLiteral("Configuracoes salvas. Reiniciando registro..."));

    if (m_config.darkTheme != wasDark) {
        brand::applyTheme(m_config.darkTheme);
        rebuildViews();   // controles releem as cores do tema na construcao
    }
    m_ramalLabel->setText(m_config.username);

    if (m_sip) {
        m_tones->stopRingback();   // garante que nenhum som siga ao reiniciar o SIP
        stopRing();
        delete m_sip;
        m_sip = nullptr;
    }
    if (m_config.isComplete()) startSip();
}

// Reconstroi as 4 telas para que os controles releiam a paleta do tema atual.
void MainWindow::rebuildViews() {
    const QString status = m_lastStatus;
    for (QWidget* v : { m_dialerView, m_incomingView, m_inCallView, m_historyView })
        if (v) v->deleteLater();
    m_dialerView = m_incomingView = m_inCallView = m_historyView = nullptr;

    setBg(this, bodyBg());
    buildViews();

    m_ramalLabel->setText(m_config.username);
    applyState(m_sip ? m_sip->state() : SipManager::LineState::Offline);
    setStatus(status);
}

// =====================================================================
//  Janela: fechar -> bandeja, posicao travada, bloqueio de minimizar/mover
// =====================================================================

void MainWindow::closeEvent(QCloseEvent* e) {
    if (m_reallyExit) { e->accept(); return; }
    if (m_ringing) { bringToForeground(); e->ignore(); return; }
    hideToTray();
    e->ignore();
}

QPoint MainWindow::lockedPos() const {
    QScreen* scr = screen() ? screen() : QApplication::primaryScreen();
    const QRect a = scr->availableGeometry();
    return { a.x() + a.width() - width() - dim::EdgeGap,
             a.y() + a.height() - height() - dim::EdgeGap };
}

void MainWindow::lockToCorner() {
    const QPoint p = lockedPos();
    if (pos() != p) move(p);
}

QPixmap MainWindow::whiteLogo() const {
    static QPixmap cached;
    if (!cached.isNull()) return cached;
    QImage img(":/assets/logo.png");
    if (img.isNull()) return {};
    img = img.convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < img.height(); ++y) {
        auto* line = reinterpret_cast<QRgb*>(img.scanLine(y));
        for (int x = 0; x < img.width(); ++x)
            line[x] = qRgba(255, 255, 255, qAlpha(line[x]));
    }
    cached = QPixmap::fromImage(img);
    return cached;
}

#ifdef Q_OS_WIN
bool MainWindow::nativeEvent(const QByteArray&, void* message, qintptr* result) {
    auto* msg = static_cast<MSG*>(message);
    if (msg->message == WM_SYSCOMMAND) {
        const WPARAM cmd = msg->wParam & 0xFFF0;
        if (cmd == SC_MINIMIZE || cmd == SC_MOVE) { if (result) *result = 0; return true; }
    } else if (msg->message == WM_WINDOWPOSCHANGING) {
        auto* wp = reinterpret_cast<WINDOWPOS*>(msg->lParam);
        const QPoint p = lockedPos();
        wp->x = p.x();
        wp->y = p.y();
    }
    return false;
}
#endif

}  // namespace sphone
