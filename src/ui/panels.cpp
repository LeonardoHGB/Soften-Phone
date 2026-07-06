#include "ui/panels.h"
#include "ui/signalwidgets.h"
#include "ui/recentsmodel.h"
#include "ui/controls.h"
#include "core/brand.h"
#include "core/version.h"
#include "data/sipconfig.h"

#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QLinearGradient>
#include <QLineEdit>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QButtonGroup>
#include <QListView>
#include <QTimer>
#include <QGraphicsOpacityEffect>
#include <QMessageBox>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QDate>
#include <QWindow>
#include <QEvent>
#include <QKeyEvent>
#include <algorithm>

using namespace brand;

namespace sphone {

// ===========================================================================
//  Helpers
// ===========================================================================
namespace {

// Iniciais a partir do nome (1a letra de ate 2 palavras); cai no numero.
QString initialsFrom(const QString& name, const QString& number) {
    const QString n = name.trimmed();
    if (!n.isEmpty()) {
        const QStringList parts = n.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        if (parts.size() >= 2) return (parts[0].left(1) + parts[1].left(1)).toUpper();
        return parts[0].left(2).toUpper();
    }
    const QString d = QString(number).remove(QRegularExpression("[^0-9]"));
    return d.right(2);
}

// QLabel com cor/fonte via folha de estilo, fundo transparente.
QLabel* mkLabel(const QString& text, const QFont& f, const QColor& c,
                Qt::Alignment align = Qt::AlignLeft | Qt::AlignVCenter,
                QWidget* parent = nullptr) {
    auto* l = new QLabel(text, parent);
    l->setFont(f);
    l->setAlignment(align);
    l->setStyleSheet(QStringLiteral("color:%1;background:transparent;").arg(c.name()));
    l->setAttribute(Qt::WA_TransparentForMouseEvents);
    return l;
}

// Marcador tecnico "— NN" (canto sup-direito dos paineis).
QLabel* mkMarker(const QString& text, QWidget* parent) {
    return mkLabel(text, fontTelemetry(8.5), textTertiary(),
                   Qt::AlignRight | Qt::AlignVCenter, parent);
}

}  // namespace

// ===========================================================================
//  TitleBar
// ===========================================================================
TitleBar::TitleBar(QWidget* parent) : QWidget(parent) {
    setFixedHeight(dim::TitleBarH);

    auto* h = new QHBoxLayout(this);
    h->setContentsMargins(18, 0, 12, 0);
    h->setSpacing(10);

    // Marca: logo oficial da Soften (cores originais) + "SOFTEN PHONE".
    auto* logo = new QLabel(this);
    QPixmap logoPix(":/assets/logo.png");
    if (!logoPix.isNull())
        logo->setPixmap(logoPix.scaled(28, 28, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    logo->setAttribute(Qt::WA_TransparentForMouseEvents);
    h->addWidget(logo);

    auto* brandBox = new QVBoxLayout();
    brandBox->setContentsMargins(0, 0, 0, 0);
    brandBox->setSpacing(0);
    brandBox->addStretch();
    brandBox->addWidget(mkLabel(QStringLiteral("SOFTEN PHONE"), fontPanelTitle(11.5), Qt::white));
    brandBox->addStretch();
    h->addLayout(brandBox);

    h->addStretch();

    m_pill = new RegPill(this);
    h->addWidget(m_pill);
    h->addSpacing(8);

    // Chrome: apenas fechar. Nao ha minimizar (esconder e proibido) e a janela
    // nao pode ser arrastada/movida — anti-esconder.
    auto mkChrome = [this](const QString& g, const QColor& hov) {
        auto* b = new QPushButton(g, this);
        b->setFont(iconPx(11));
        b->setFixedSize(34, 30);
        b->setCursor(Qt::PointingHandCursor);
        b->setStyleSheet(QStringLiteral(
            "QPushButton{border:none;background:transparent;color:#CFEAFB;border-radius:7px;}"
            "QPushButton:hover{background:%1;color:#ffffff;}").arg(hov.name()));
        return b;
    };
    m_close = mkChrome(QString(QChar(0xE8BB)), QColor(0xE2, 0x45, 0x3D));
    connect(m_close, &QPushButton::clicked, this, &TitleBar::closeClicked);
    h->addWidget(m_close);
}

void TitleBar::setRegistered(bool ok, const QString& text) { m_pill->setRegistered(ok, text); }

void TitleBar::setLocked(bool locked) {
    m_locked = locked;
    // Chrome desabilitado e esmaecido enquanto travado (chamada recebida).
    const double op = locked ? 0.35 : 1.0;
    for (QPushButton* b : { m_close }) {
        if (!b) continue;
        b->setEnabled(!locked);
        auto* eff = qobject_cast<QGraphicsOpacityEffect*>(b->graphicsEffect());
        if (!eff) { eff = new QGraphicsOpacityEffect(b); b->setGraphicsEffect(eff); }
        eff->setOpacity(op);
    }
}

void TitleBar::paintEvent(QPaintEvent*) {
    QPainter g(this);
    g.setRenderHint(QPainter::Antialiasing);
    // Header grafite (esq -> dir).
    QLinearGradient grad(0, 0, width(), 0);
    grad.setColorAt(0.0,  QColor(0x1C, 0x1D, 0x22));
    grad.setColorAt(0.5,  QColor(0x24, 0x25, 0x2B));
    grad.setColorAt(1.0,  QColor(0x2C, 0x2D, 0x34));
    g.fillRect(rect(), grad);

    // Faixa tricolor (3px) na base do header: branco | azul | branco.
    const double w = width();
    const QRectF base(0, height() - 3, w, 3);
    g.setPen(Qt::NoPen);
    g.fillRect(base, QColor(0xFF, 0xFF, 0xFF));                                       // branco (bordas)
    g.fillRect(QRectF(w / 3.0, base.top(), w / 3.0, 3), QColor(0x00, 0x9B, 0xDB));    // azul Soften (centro)
}

// A janela nao pode ser arrastada pela barra de titulo (anti-esconder): sem
// handlers de mouse, a TitleBar nao move o top-level.

// ===========================================================================
//  NavRail
// ===========================================================================
NavRail::NavRail(QWidget* parent) : QWidget(parent) {
    setFixedWidth(dim::RailW);

    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(0, 16, 0, 16);
    v->setSpacing(8);
    v->setAlignment(Qt::AlignHCenter);

    auto mkItem = [this](const QString& glyph, bool active) {
        auto* b = new RoundGlyphButton(this);
        b->shape = RoundGlyphButton::Shape::RoundedSquare;
        b->squareRadius = 12;
        b->setFixedSize(48, 44);
        b->glyph = glyph;
        b->glyphSize = 17;
        b->idleGlyph = QColor(0x7A, 0x7B, 0x82);          // cinza grafite
        b->idleFill = Qt::transparent;
        b->activeFill = QColor(0x2A, 0x25, 0x19);          // dourado bem escuro
        b->activeGlyph = QColor(0xD4, 0xAF, 0x37);         // dourado
        b->setActive(active);
        return b;
    };

    m_keypadBtn = mkItem(glyph::Dialpad, true);
    m_recentsBtn = mkItem(glyph::History, false);
    m_settingsBtn = mkItem(glyph::Settings, false);
    connect(m_keypadBtn,   &RoundGlyphButton::clicked, this, &NavRail::goHome);
    connect(m_recentsBtn,  &RoundGlyphButton::clicked, this, &NavRail::toggleRecents);
    connect(m_settingsBtn, &RoundGlyphButton::clicked, this, &NavRail::toggleSettings);
    v->addWidget(m_keypadBtn, 0, Qt::AlignHCenter);
    v->addWidget(m_recentsBtn, 0, Qt::AlignHCenter);
    v->addWidget(m_settingsBtn, 0, Qt::AlignHCenter);

    v->addStretch();

    // Botao de tema (claro/escuro) removido por ora — tema fixo grafite.
    // O sinal toggleTheme permanece para quando voltar.

    v->addSpacing(6);
    // Avatar do usuario (silhueta). O ramal agora aparece na pilula do header.
    m_badge = new QLabel(glyph::Contact, this);
    m_badge->setFixedSize(40, 40);
    m_badge->setAlignment(Qt::AlignCenter);
    m_badge->setFont(iconPx(18));
    m_badge->setStyleSheet(QStringLiteral(
        "color:#dceaf5;"
        "background:qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #4a4226, stop:1 #2a2519);"
        "border:2px solid #d4af37;border-radius:20px;"));
    v->addWidget(m_badge, 0, Qt::AlignHCenter);
}

void NavRail::setRamal(const QString& /*ramal*/) {
    // Avatar fixo (silhueta); o ramal e exibido na pilula de registro do header.
}

void NavRail::setActive(Active a) {
    if (m_keypadBtn)   m_keypadBtn->setActive(a == Active::Home);
    if (m_recentsBtn)  m_recentsBtn->setActive(a == Active::Recents);
    if (m_settingsBtn) m_settingsBtn->setActive(a == Active::Settings);
}

void NavRail::paintEvent(QPaintEvent*) {
    QPainter g(this);
    // Sidebar grafite quase preto (gradiente vertical sutil).
    QLinearGradient grad(0, 0, 0, height());
    grad.setColorAt(0.0, QColor(0x0D, 0x0E, 0x11));
    grad.setColorAt(1.0, QColor(0x0A, 0x0B, 0x0D));
    g.fillRect(rect(), grad);
    g.setPen(QPen(QColor(0x1A, 0x1B, 0x1F), 1));
    g.drawLine(width() - 1, 0, width() - 1, height());
}

// ===========================================================================
//  DialerPanel
// ===========================================================================
DialerPanel::DialerPanel(QWidget* parent) : QWidget(parent) {
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(dim::PanelPad, 24, dim::PanelPad, 24);
    v->setSpacing(0);

    // Cabecalho: titulo + sub | marcador.
    auto* head = new QHBoxLayout();
    head->setContentsMargins(0, 0, 0, 0);
    auto* titleBox = new QVBoxLayout();
    titleBox->setSpacing(2);
    titleBox->addWidget(mkLabel(QStringLiteral("Discador"), fontPanelTitle(17), QColor(0xEE, 0xF4, 0xFB)));
    titleBox->addWidget(mkLabel(QStringLiteral("TECLADO · SIP/RTP"), fontTelemetry(8), QColor(0x8A, 0x8B, 0x92)));
    head->addLayout(titleBox);
    head->addStretch();
    v->addLayout(head);
    v->addSpacing(20);

    // Display: card com numero (mono) + backspace.
    auto* card = new QWidget(this);
    card->setObjectName("DialDisplay");
    card->setFixedHeight(66);
    card->setStyleSheet(QStringLiteral(
        "#DialDisplay{background:qlineargradient(x1:0,y1:0,x2:1,y2:0,"
        " stop:0 #1d1e23, stop:1 #17181c);border:1px solid #34353c;border-radius:%1px;}")
        .arg(dim::CardRadius));
    auto* ch = new QHBoxLayout(card);
    ch->setContentsMargins(18, 6, 10, 6);
    m_display = new QLineEdit(card);
    m_display->setFont(fontDisplay(23));
    m_display->setPlaceholderText(QStringLiteral("número"));
    m_display->setStyleSheet(QStringLiteral(
        "QLineEdit{background:transparent;border:none;color:#eef4fb;}"));
    QPalette dp = m_display->palette();
    dp.setColor(QPalette::PlaceholderText, QColor(0x5A, 0x5B, 0x62));
    m_display->setPalette(dp);
    m_display->setValidator(new QRegularExpressionValidator(
        QRegularExpression(QStringLiteral("[0-9*#+ ]*")), m_display));
    connect(m_display, &QLineEdit::returnPressed, this, &DialerPanel::callRequested);
    m_display->installEventFilter(this);
    ch->addWidget(m_display, 1);

    auto* back = new RoundGlyphButton(card);
    back->setFixedSize(40, 40);
    back->glyph = glyph::Backspace;
    back->glyphSize = 16;
    back->idleGlyph = textSecondary();
    connect(back, &RoundGlyphButton::clicked, this, [this] {
        m_display->backspace();
    });
    ch->addWidget(back, 0);
    v->addWidget(card);
    v->addSpacing(18);

    // Teclado 3x4.
    auto* grid = new QGridLayout();
    grid->setSpacing(12);
    struct K { const char* k; const char* l; };
    static const K keys[12] = {
        {"1", ""}, {"2", "ABC"}, {"3", "DEF"},
        {"4", "GHI"}, {"5", "JKL"}, {"6", "MNO"},
        {"7", "PQRS"}, {"8", "TUV"}, {"9", "WXYZ"},
        {"*", ""}, {"0", "+"}, {"#", ""}
    };
    for (int i = 0; i < 12; ++i) {
        auto* key = new KeypadButton(this);
        key->keyChar = QString::fromUtf8(keys[i].k);
        key->letters = QString::fromUtf8(keys[i].l);
        key->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        key->setMinimumSize(56, 56);
        const QString kc = key->keyChar;
        connect(key, &KeypadButton::clicked, this, [this, kc] {
            if (!kc.isEmpty()) emit keyTone(kc[0]);
            append(kc);
        });
        grid->addWidget(key, i / 3, i % 3);
    }
    v->addLayout(grid, 1);
    v->addSpacing(16);

    // Acao: botao chamar centralizado, com halo pulsante.
    auto* actions = new QHBoxLayout();
    actions->setContentsMargins(0, 0, 0, 0);
    actions->addStretch();
    m_call = new CallButton(this);
    m_call->setFixedSize(72, 72);
    m_call->top  = QColor(0xF5, 0xD7, 0x7A);   // dourado claro (topo)
    m_call->base = QColor(0xD4, 0xAF, 0x37);   // dourado (base)
    m_call->glyphColor = QColor(0x15, 0x16, 0x1A);  // handset escuro sobre o dourado
    m_call->glyph = glyph::Phone;
    m_call->glow = false;          // sem halo/fade em volta
    connect(m_call, &CallButton::clicked, this, &DialerPanel::callRequested);
    actions->addWidget(m_call);
    actions->addStretch();
    v->addLayout(actions);
}

QString DialerPanel::number() const { return m_display->text().trimmed(); }
void DialerPanel::setNumber(const QString& n) {
    m_display->setText(n);
    m_display->setCursorPosition(n.length());
}
void DialerPanel::focusDisplay() { m_display->setFocus(); }
void DialerPanel::setCallEnabled(bool on) { m_call->setEnabled(on); }

void DialerPanel::append(const QString& s) {
    const int pos = m_display->cursorPosition();
    QString t = m_display->text();
    t.insert(pos, s);
    m_display->setText(t);
    m_display->setCursorPosition(pos + s.length());
}

void DialerPanel::paintEvent(QPaintEvent*) {
    QPainter g(this);
    QLinearGradient bg(rect().topLeft(), rect().bottomLeft());
    bg.setColorAt(0.0, QColor(0x15, 0x16, 0x1a));
    bg.setColorAt(0.5, QColor(0x19, 0x1a, 0x1f));
    bg.setColorAt(1.0, QColor(0x10, 0x11, 0x15));
    g.fillRect(rect(), bg);
}

bool DialerPanel::eventFilter(QObject* watched, QEvent* event) {
    if (watched == m_display && event->type() == QEvent::KeyPress) {
        const QString txt = static_cast<QKeyEvent*>(event)->text();
        if (txt.size() == 1) {
            const QChar c = txt.at(0);
            if ((c >= '0' && c <= '9') || c == '*' || c == '#' || c == '+')
                emit keyTone(c);
        }
    }
    return QWidget::eventFilter(watched, event);
}

// ===========================================================================
//  CallPanel
// ===========================================================================
CallPanel::CallPanel(QWidget* parent) : QWidget(parent) {
    m_rings = new SignalRings(this);
    m_rings->color = sig().cyan;

    m_avatar = new RingAvatar(this);
    m_avatar->base = sig().cyan;
    m_avatar->gradient = true;

    m_nameLabel  = mkLabel(QString(), fontPanelTitle(18), sig().onField,
                           Qt::AlignHCenter | Qt::AlignVCenter, this);
    m_numberLabel = mkLabel(QString(), fontDisplay(11), sig().onFieldSub,
                            Qt::AlignHCenter | Qt::AlignVCenter, this);

    m_timerPill = new QLabel(QStringLiteral("00:00"), this);
    m_timerPill->setAlignment(Qt::AlignCenter);
    m_timerPill->setFont(fontDisplay(13));
    m_timerPill->setStyleSheet(QStringLiteral(
        "color:%1;background:rgba(255,255,255,28);border-radius:13px;padding:3px 14px;")
        .arg(sig().onField.name()));

    m_wave = new Waveform(this);
    m_wave->color = sig().cyan;

    // Titulo do painel (canto sup-esq, como os demais).
    auto* title = mkLabel(QStringLiteral("Chamada ativa"), fontPanelTitle(17), sig().onField,
                          Qt::AlignLeft | Qt::AlignVCenter, this);
    title->setObjectName("CallTitle");
    auto* tsub = mkLabel(QStringLiteral("EM CURSO · CRIPTOGRAFADA"), fontTelemetry(8), sig().cyanLight,
                         Qt::AlignLeft | Qt::AlignVCenter, this);
    tsub->setObjectName("CallTitleSub");
    auto* marker = mkLabel(QStringLiteral("— 02"), fontTelemetry(8.5), sig().onFieldSub,
                           Qt::AlignRight | Qt::AlignVCenter, this);
    marker->setObjectName("CallMarker");

    // Cluster 2x3 (Active/Held).
    m_cluster = new QWidget(this);
    auto* cg = new QGridLayout(m_cluster);
    cg->setContentsMargins(0, 0, 0, 0);
    cg->setHorizontalSpacing(26);
    cg->setVerticalSpacing(6);
    auto mkCtrl = [this](const QString& gl, const QString& lab, RoundGlyphButton** out) {
        auto* cell = new QWidget(m_cluster);
        auto* cv = new QVBoxLayout(cell);
        cv->setContentsMargins(0, 0, 0, 0);
        cv->setSpacing(6);
        auto* b = new RoundGlyphButton(cell);
        b->setFixedSize(52, 52);
        b->glyph = gl; b->glyphSize = 19;
        b->idleFill = QColor(255, 255, 255, 18);
        b->idleGlyph = sig().onField;
        b->activeFill = sig().cyan;
        b->activeGlyph = Qt::white;
        cv->addWidget(b, 0, Qt::AlignHCenter);
        cv->addWidget(mkLabel(lab, fontLabel(8.5), sig().onFieldSub,
                              Qt::AlignHCenter | Qt::AlignVCenter, cell));
        if (out) *out = b;
        return cell;
    };
    RoundGlyphButton* dummy = nullptr;
    auto* cMute = mkCtrl(glyph::Microphone, QStringLiteral("Mudo"), &m_muteBtn);
    auto* cKeys = mkCtrl(glyph::Dialpad,    QStringLiteral("Teclado"), &dummy);
    auto* cHold = mkCtrl(glyph::Pause,      QStringLiteral("Espera"), &m_holdBtn);
    auto* cTrf  = mkCtrl(glyph::Transfer,   QStringLiteral("Transferir"), &dummy);
    cg->addWidget(cMute, 0, 0); cg->addWidget(cKeys, 0, 1);
    cg->addWidget(cHold, 1, 0); cg->addWidget(cTrf, 1, 1);
    // Mudo/Espera NAO sao checkable: o estado ativo e dirigido pela MainWindow
    // (setMuteActive/setHoldActive) apos confirmar com o SIP, evitando duplo-toggle.
    connect(m_muteBtn, &RoundGlyphButton::clicked, this, &CallPanel::muteClicked);
    connect(m_holdBtn, &RoundGlyphButton::clicked, this, &CallPanel::holdClicked);
    // O clique nas celulas correto: o botao e quem emite (label so visual).
    if (auto* b = cTrf->findChild<RoundGlyphButton*>())
        connect(b, &RoundGlyphButton::clicked, this, &CallPanel::transferClicked);

    // Encerrar (vermelho).
    m_hangup = new CallButton(this);
    m_hangup->setFixedSize(66, 66);
    m_hangup->base = sig().red;
    m_hangup->glyph = glyph::Phone;
    m_hangup->glyphRotation = 135;
    connect(m_hangup, &CallButton::clicked, this, &CallPanel::hangupRequested);

    // Incoming (Atender/Recusar).
    m_incoming = new QWidget(this);
    auto* ih = new QHBoxLayout(m_incoming);
    ih->setContentsMargins(0, 0, 0, 0);
    ih->setSpacing(40);
    auto* reject = new CallButton(m_incoming);
    reject->setFixedSize(64, 64);
    reject->base = sig().red; reject->glyph = glyph::Phone; reject->glyphRotation = 135;
    auto* answer = new CallButton(m_incoming);
    answer->setFixedSize(64, 64);
    answer->base = sig().green; answer->glyph = glyph::Phone;
    connect(reject, &CallButton::clicked, this, &CallPanel::rejectRequested);
    connect(answer, &CallButton::clicked, this, &CallPanel::answerRequested);
    ih->addStretch(); ih->addWidget(reject); ih->addWidget(answer); ih->addStretch();

    // Idle hint.
    m_idleHint = new QWidget(this);
    auto* idv = new QVBoxLayout(m_idleHint);
    idv->setContentsMargins(0, 0, 0, 0);
    idv->addWidget(mkLabel(QStringLiteral("Sem chamada ativa"), fontLabel(11), sig().onFieldSub,
                           Qt::AlignHCenter | Qt::AlignVCenter, m_idleHint));

    setView(View::Idle);
}

void CallPanel::setPeer(const QString& name, const QString& number) {
    m_name = name; m_number = number;
    m_nameLabel->setText(name.isEmpty() ? (number.isEmpty() ? QStringLiteral("Desconhecido") : number) : name);
    m_numberLabel->setText(name.isEmpty() ? QString() : number);
    m_avatar->initials = initialsFrom(name, number);
    m_avatar->update();
}

void CallPanel::setTimerText(const QString& t) { m_timerPill->setText(t); }
void CallPanel::setMuteActive(bool on) { if (m_muteBtn) { m_muteBtn->setActive(on); m_muteBtn->glyph = on ? glyph::Mute : glyph::Microphone; m_muteBtn->update(); } }
void CallPanel::setHoldActive(bool on) { if (m_holdBtn) m_holdBtn->setActive(on); }
void CallPanel::pushAudioLevel(float v) { if (m_wave && m_wave->isVisible()) m_wave->pushLevel(v); }
void CallPanel::resetControls() { setMuteActive(false); setHoldActive(false); }

void CallPanel::setView(View v) {
    m_view = v;
    const bool active   = (v == View::Active || v == View::Held);
    const bool incoming = (v == View::Incoming);
    const bool outgoing = (v == View::Outgoing);
    const bool idle     = (v == View::Idle);

    m_idleHint->setVisible(idle);
    m_rings->setVisible(!idle);
    m_avatar->setVisible(!idle);
    m_nameLabel->setVisible(!idle);
    m_numberLabel->setVisible(!idle);
    m_timerPill->setVisible(active);
    m_wave->setVisible(active);
    m_cluster->setVisible(active);
    m_hangup->setVisible(active || outgoing);
    m_incoming->setVisible(incoming);

    m_rings->setActive(active || outgoing);
    m_wave->setPaused(v == View::Held);

    // O status vai no sub do cabecalho ("Chamada ativa / <status>").
    if (auto* sub = findChild<QLabel*>("CallTitleSub")) {
        sub->setVisible(!idle);
        sub->setText(incoming ? QStringLiteral("CHAMADA RECEBIDA")
                   : outgoing ? QStringLiteral("CHAMANDO…")
                   : v == View::Held ? QStringLiteral("EM ESPERA")
                   : QStringLiteral("EM CURSO · CRIPTOGRAFADA"));
    }
    relayout();
    // Reposiciona depois do layout pai assentar o tamanho do painel. Sem isto,
    // trocar de tema DURANTE a chamada (rebuild do shell) posiciona os botoes com
    // o tamanho antigo/zero e eles "quebram" ate o proximo resize.
    QTimer::singleShot(0, this, [this] { relayout(); });
}

void CallPanel::paintEvent(QPaintEvent*) {
    QPainter g(this);
    QLinearGradient grad(width() * 0.2, 0, width() * 0.8, height());
    grad.setColorAt(0.0, sig().fieldA);
    grad.setColorAt(0.5, sig().fieldB);
    grad.setColorAt(1.0, sig().fieldC);
    g.fillRect(rect(), grad);
}

void CallPanel::resizeEvent(QResizeEvent*) { relayout(); }

void CallPanel::relayout() {
    const int w = width(), h = height();
    const int cx = w / 2;

    // Cabecalho.
    if (auto* t = findChild<QLabel*>("CallTitle"))    t->setGeometry(dim::PanelPad, 22, w - 2*dim::PanelPad, 24);
    if (auto* s = findChild<QLabel*>("CallTitleSub")) s->setGeometry(dim::PanelPad, 47, w - 2*dim::PanelPad, 16);
    if (auto* m = findChild<QLabel*>("CallMarker"))   m->setGeometry(w - dim::PanelPad - 60, 22, 60, 16);

    if (m_idleHint->isVisible()) {
        m_idleHint->setGeometry(0, h/2 - 20, w, 40);
        return;
    }

    // Pilha vertical centrada: aneis+avatar, nome, numero, timer, waveform.
    const int ringSize = std::min(w - 80, 196);
    const int avSize = 88;
    int y = 78;
    m_rings->setGeometry(cx - ringSize/2, y, ringSize, ringSize);
    m_avatar->setGeometry(cx - avSize/2, y + ringSize/2 - avSize/2, avSize, avSize);
    y += ringSize + 6;

    m_nameLabel->setGeometry(20, y, w - 40, 28); y += 30;
    m_numberLabel->setGeometry(20, y, w - 40, 18); y += 24;

    const int pillW = 130;
    m_timerPill->setGeometry(cx - pillW/2, y, pillW, 28);
    if (m_timerPill->isVisible()) y += 38;

    if (m_wave->isVisible()) m_wave->setGeometry(28, y, w - 56, 36);

    // Rodape ancorado embaixo: cluster acima do encerrar (gap garantido); ou
    // incoming (Atender/Recusar); ou so o encerrar (Outgoing).
    if (m_hangup->isVisible())
        m_hangup->setGeometry(cx - 33, h - 80, 66, 66);
    if (m_cluster->isVisible()) {
        const QSize cs = m_cluster->sizeHint();
        m_cluster->setGeometry(cx - cs.width()/2, h - 80 - cs.height() - 14, cs.width(), cs.height());
    }
    if (m_incoming->isVisible())
        m_incoming->setGeometry(0, h - 150, w, 80);
}

// ===========================================================================
//  RecentsPanel
// ===========================================================================
RecentsPanel::RecentsPanel(QWidget* parent) : QWidget(parent) {
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(dim::PanelPad, 24, dim::PanelPad, 16);
    v->setSpacing(0);

    auto* head = new QHBoxLayout();
    auto* titleBox = new QVBoxLayout();
    titleBox->setSpacing(2);
    titleBox->addWidget(mkLabel(QStringLiteral("Recentes"), fontPanelTitle(17), textPrimary()));
    titleBox->addWidget(mkLabel(QStringLiteral("HISTÓRICO · CONTATOS"), fontTelemetry(8), sig().accentSub));
    head->addLayout(titleBox);
    head->addStretch();
    v->addLayout(head);
    v->addSpacing(16);

    // Busca.
    auto* searchCard = new QWidget(this);
    searchCard->setObjectName("SearchCard");
    searchCard->setFixedHeight(44);
    searchCard->setStyleSheet(QStringLiteral(
        "#SearchCard{background:%1;border:1px solid %2;border-radius:%3px;}")
        .arg(panelGray().name(), border().name()).arg(dim::CardRadius));
    auto* sh = new QHBoxLayout(searchCard);
    sh->setContentsMargins(14, 0, 14, 0);
    sh->setSpacing(8);
    auto* sIcon = mkLabel(glyph::Search, iconPx(14), textTertiary(),
                          Qt::AlignVCenter | Qt::AlignHCenter, searchCard);
    sIcon->setFixedWidth(18);
    sh->addWidget(sIcon);
    m_search = new QLineEdit(searchCard);
    m_search->setFont(fontLabel(10.5));
    m_search->setPlaceholderText(QStringLiteral("Buscar contato ou número"));
    m_search->setStyleSheet(QStringLiteral(
        "QLineEdit{background:transparent;border:none;color:%1;}").arg(textPrimary().name()));
    connect(m_search, &QLineEdit::textChanged, this, [this](const QString& t) {
        if (m_proxy) m_proxy->setSearch(t);
    });
    sh->addWidget(m_search, 1);
    v->addWidget(searchCard);
    v->addSpacing(14);

    // Abas de filtro.
    auto* tabsRow = new QHBoxLayout();
    tabsRow->setSpacing(6);
    m_tabs = new QButtonGroup(this);
    const char* names[] = { "Todas", "Perdidas", "Recebidas", "Feitas" };
    for (int i = 0; i < 4; ++i) {
        auto* b = new QPushButton(QString::fromUtf8(names[i]), this);
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setFont(fontLabel(9.5));
        b->setStyleSheet(QStringLiteral(
            "QPushButton{border:none;background:transparent;color:%1;padding:5px 12px;border-radius:13px;}"
            "QPushButton:checked{background:%2;color:#ffffff;}")
            .arg(textSecondary().name(), sig().navyA.name()));
        m_tabs->addButton(b, i);
        tabsRow->addWidget(b);
    }
    m_tabs->button(0)->setChecked(true);   // "Todas" por padrao
    tabsRow->addStretch();
    connect(m_tabs, &QButtonGroup::idClicked, this, [this](int id) {
        if (m_proxy) m_proxy->setMode(static_cast<CallLogProxy::Mode>(id));
    });
    v->addLayout(tabsRow);
    v->addSpacing(10);

    // Lista (Model/View): CallLogModel -> CallLogProxy -> QListView + delegate.
    m_model = new CallLogModel(this);
    m_proxy = new CallLogProxy(this);
    m_proxy->setSourceModel(m_model);
    auto* view = new QListView(this);
    view->setModel(m_proxy);
    auto* delegate = new CallLogDelegate(view);
    view->setItemDelegate(delegate);
    view->setFrameShape(QFrame::NoFrame);
    view->setSelectionMode(QAbstractItemView::NoSelection);
    view->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    view->setMouseTracking(true);                 // habilita State_MouseOver no delegate
    view->viewport()->setAttribute(Qt::WA_Hover);
    view->setCursor(Qt::PointingHandCursor);
    view->setStyleSheet(QStringLiteral(
        "QListView{background:transparent;outline:none;}"
        "QScrollBar:vertical{width:8px;background:transparent;}"
        "QScrollBar::handle:vertical{background:%1;border-radius:4px;}"
        "QScrollBar::add-line,QScrollBar::sub-line{height:0;}").arg(border().name()));
    connect(delegate, &CallLogDelegate::redial, this, &RecentsPanel::redial);
    v->addWidget(view, 1);

    // Rodape de telemetria (estatico — fase 2 puxa dados reais).
    v->addSpacing(8);
    auto* foot = new QHBoxLayout();
    foot->setSpacing(0);
    auto mkStat = [this](const QString& cap, const QString& val, QLabel** out) {
        auto* cell = new QVBoxLayout();
        cell->setSpacing(1);
        cell->addWidget(mkLabel(cap, fontTelemetry(7.5), textTertiary()));
        auto* vl = mkLabel(val, fontDisplay(10.5), textSecondary());
        if (out) *out = vl;
        cell->addWidget(vl);
        return cell;
    };
    foot->addLayout(mkStat(QStringLiteral("CODEC"), QStringLiteral("—"), &m_codecVal));
    foot->addStretch();
    foot->addLayout(mkStat(QStringLiteral("LATÊNCIA"), QStringLiteral("—"), &m_latVal));
    foot->addStretch();
    foot->addLayout(mkStat(QStringLiteral("SINAL"), QStringLiteral("▯▯▯▯"), &m_sigVal));
    v->addLayout(foot);
}

void RecentsPanel::setEntries(const QList<CallAudit>& items) { m_model->setItems(items); }

void RecentsPanel::setTelemetry(const QString& codec, const QString& latency, const QString& signal) {
    if (m_codecVal) m_codecVal->setText(codec);
    if (m_latVal)   m_latVal->setText(latency);
    if (m_sigVal)   m_sigVal->setText(signal);
}

void RecentsPanel::clearTelemetry() {
    setTelemetry(QStringLiteral("—"), QStringLiteral("—"), QStringLiteral("▯▯▯▯"));
}

void RecentsPanel::paintEvent(QPaintEvent*) {
    QPainter g(this);
    QLinearGradient bg(rect().topLeft(), rect().bottomLeft());
    bg.setColorAt(0.0, QColor(0x15, 0x16, 0x1a));
    bg.setColorAt(0.5, QColor(0x19, 0x1a, 0x1f));
    bg.setColorAt(1.0, QColor(0x10, 0x11, 0x15));
    g.fillRect(rect(), bg);
}

// ===========================================================================
//  SettingsPanel
// ===========================================================================
SettingsPanel::SettingsPanel(SipConfig* config, QWidget* parent)
    : QWidget(parent), m_config(config) {
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(dim::PanelPad, 24, dim::PanelPad, 20);
    v->setSpacing(0);

    // Cabecalho.
    auto* head = new QHBoxLayout();
    auto* titleBox = new QVBoxLayout();
    titleBox->setSpacing(2);
    titleBox->addWidget(mkLabel(QStringLiteral("Configurações"), fontPanelTitle(17), textPrimary()));
    titleBox->addWidget(mkLabel(QStringLiteral("CONTA · SIP/RTP"), fontTelemetry(8), sig().accentSub));
    head->addLayout(titleBox);
    head->addStretch();
    head->addWidget(mkMarker(QString::fromUtf8("v") + QStringLiteral(SPHONE_VERSION), this), 0, Qt::AlignTop);
    v->addLayout(head);
    v->addSpacing(18);

    auto section = [&](const QString& t) {
        auto* l = mkLabel(t.toUpper(), fontTelemetry(8), textTertiary());
        v->addWidget(l);
        v->addSpacing(8);
    };
    auto field = [&](const QString& label, bool password, QLineEdit*& out) {
        v->addWidget(mkLabel(label, fontLabel(9.5), textSecondary()));
        v->addSpacing(4);
        out = new QLineEdit(this);
        out->setFont(fontLabel(11));
        out->setFixedHeight(42);
        if (password) out->setEchoMode(QLineEdit::Password);
        out->setStyleSheet(QStringLiteral(
            "QLineEdit{background:%1;border:1px solid %2;border-radius:%3px;padding:0 13px;color:%4;}"
            "QLineEdit:focus{border:1px solid %5;}")
            .arg(panelGray().name(), border().name()).arg(dim::CardRadius)
            .arg(textPrimary().name(), sig().cyan.name()));
        v->addWidget(out);
        v->addSpacing(12);
    };

    auto combo = [&](const QString& label, QComboBox*& out) {
        v->addWidget(mkLabel(label, fontLabel(9.5), textSecondary()));
        v->addSpacing(4);
        out = new QComboBox(this);
        out->setFont(fontLabel(11));
        out->setFixedHeight(42);
        out->setCursor(Qt::PointingHandCursor);
        out->setStyleSheet(QStringLiteral(
            "QComboBox{background:%1;border:1px solid %2;border-radius:%3px;padding:0 13px;color:%4;}"
            "QComboBox:focus{border:1px solid %5;}"
            "QComboBox::drop-down{border:none;width:26px;}"
            "QComboBox QAbstractItemView{background:%1;border:1px solid %2;color:%4;"
            "selection-background-color:%5;outline:none;}")
            .arg(panelGray().name(), border().name()).arg(dim::CardRadius)
            .arg(textPrimary().name(), sig().cyan.name()));
        v->addWidget(out);
        v->addSpacing(12);
    };

    section(QStringLiteral("Conta"));
    field(QStringLiteral("Servidor"), false, m_server);
    field(QStringLiteral("Ramal"), false, m_user);
    field(QStringLiteral("Senha"), true, m_pass);

    v->addSpacing(16);
    section(QStringLiteral("Áudio"));
    combo(QString::fromUtf8("Microfone (falar)"), m_capture);
    combo(QString::fromUtf8("Alto-falante / fone (ouvir)"), m_playback);

    // Secao "Aparência" (alternar claro/escuro) removida por ora — tema fixo
    // grafite. Voltara no futuro.

    v->addSpacing(16);
    section(QStringLiteral("Atualização"));
    {
        auto* upd = new QPushButton(QString::fromUtf8("Procurar atualização"), this);
        upd->setCursor(Qt::PointingHandCursor);
        upd->setFont(fontLabel(10.5));
        upd->setFixedHeight(42);
        upd->setStyleSheet(QStringLiteral(
            "QPushButton{background:%1;border:1px solid %2;border-radius:%3px;color:%4;}"
            "QPushButton:hover{border:1px solid %5;}")
            .arg(panelGray().name(), border().name()).arg(dim::CardRadius)
            .arg(textPrimary().name(), sig().cyan.name()));
        connect(upd, &QPushButton::clicked, this, &SettingsPanel::checkUpdate);
        v->addWidget(upd);
    }

    v->addStretch();

    // Rodape: Cancelar | Salvar.
    auto* foot = new QHBoxLayout();
    foot->setSpacing(10);
    foot->addStretch();
    auto* cancel = new QPushButton(QStringLiteral("Cancelar"), this);
    cancel->setCursor(Qt::PointingHandCursor);
    cancel->setFont(fontLabel(10.5));
    cancel->setFixedSize(110, 42);
    cancel->setStyleSheet(QStringLiteral(
        "QPushButton{background:%1;border:1px solid %2;border-radius:%3px;color:%4;}")
        .arg(panelGray().name(), border().name()).arg(dim::CardRadius).arg(textPrimary().name()));
    connect(cancel, &QPushButton::clicked, this, &SettingsPanel::closed);
    auto* save = new QPushButton(QStringLiteral("Salvar"), this);
    save->setCursor(Qt::PointingHandCursor);
    save->setFont(fontLabel(10.5));
    save->setFixedSize(140, 42);
    save->setStyleSheet(QStringLiteral(
        "QPushButton{background:%1;border:none;border-radius:%2px;color:#ffffff;}"
        "QPushButton:hover{background:%3;}")
        .arg(sig().cyan.name()).arg(dim::CardRadius).arg(blend(sig().cyan, Qt::white, 0.10).name()));
    connect(save, &QPushButton::clicked, this, [this] {
        if (m_server->text().trimmed().isEmpty() || m_user->text().trimmed().isEmpty()
            || m_pass->text().trimmed().isEmpty()) {
            QMessageBox::warning(this, QString::fromUtf8("Campos obrigatórios"),
                QString::fromUtf8("Preencha ao menos Servidor, Ramal e Senha."));
            return;
        }
        m_config->server         = m_server->text().trimmed();
        m_config->username       = m_user->text().trimmed();
        m_config->password       = m_pass->text();
        // O nome do device fica em itemData; "" (Padrao do sistema) zera a escolha.
        m_config->captureDevice  = m_capture  ? m_capture->currentData().toString()  : QString();
        m_config->playbackDevice = m_playback ? m_playback->currentData().toString() : QString();
        emit saved();
    });
    foot->addWidget(cancel);
    foot->addWidget(save);
    v->addLayout(foot);

    loadConfig();
    setAudioDevices({});   // semeia "Padrao do sistema" + seleciona o do config
}

void SettingsPanel::loadConfig() {
    if (!m_config) return;
    m_server->setText(m_config->server);
    m_user->setText(m_config->username);
    m_pass->setText(m_config->password);
}

void SettingsPanel::setAudioDevices(const QList<AudioDevice>& devices) {
    if (!m_capture || !m_playback) return;

    // Reconstroi as duas listas. itemData() guarda o NOME do device (o que vai pro
    // config); o item 0 e sempre "Padrao do sistema" com data vazia.
    auto fill = [&](QComboBox* box, bool wantCapture, const QString& selected) {
        box->blockSignals(true);
        box->clear();
        box->addItem(QString::fromUtf8("Padrão do sistema"), QString());
        int pick = 0;
        for (const AudioDevice& d : devices) {
            const bool ok = wantCapture ? d.capture : d.playback;
            if (!ok) continue;
            box->addItem(d.name, d.name);
            if (!selected.isEmpty() && d.name == selected)
                pick = box->count() - 1;
        }
        // Device gravado some da lista (ex: USB desplugado): mostra mesmo assim,
        // marcado, para o atendente entender por que o audio caiu no padrao.
        if (pick == 0 && !selected.isEmpty()) {
            box->addItem(selected + QString::fromUtf8("  (indisponível)"), selected);
            pick = box->count() - 1;
        }
        box->setCurrentIndex(pick);
        box->blockSignals(false);
    };

    fill(m_capture,  true,  m_config ? m_config->captureDevice  : QString());
    fill(m_playback, false, m_config ? m_config->playbackDevice : QString());
}

void SettingsPanel::paintEvent(QPaintEvent*) {
    QPainter g(this);
    QLinearGradient bg(rect().topLeft(), rect().bottomLeft());
    bg.setColorAt(0.0, QColor(0x15, 0x16, 0x1a));
    bg.setColorAt(0.5, QColor(0x19, 0x1a, 0x1f));
    bg.setColorAt(1.0, QColor(0x10, 0x11, 0x15));
    g.fillRect(rect(), bg);
}

}  // namespace sphone
