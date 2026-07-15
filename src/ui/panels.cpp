#include "ui/panels.h"
#include "ui/signalwidgets.h"
#include "ui/recentsmodel.h"
#include "core/brand.h"
#include "core/version.h"
#include "data/sipconfig.h"

#include <QPainter>
#include <QPainterPath>
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

// Fundo grafite padrao das paginas (gradiente vertical).
void paintPageBg(QWidget* w) {
    QPainter g(w);
    QLinearGradient bg(w->rect().topLeft(), w->rect().bottomLeft());
    bg.setColorAt(0.0, QColor(0x15, 0x16, 0x1a));
    bg.setColorAt(0.5, QColor(0x19, 0x1a, 0x1f));
    bg.setColorAt(1.0, QColor(0x10, 0x11, 0x15));
    g.fillRect(w->rect(), bg);
}

// Esmaece + desabilita um botao (chrome/abas durante o toque).
void dimButton(QPushButton* b, bool locked) {
    if (!b) return;
    b->setEnabled(!locked);
    auto* eff = qobject_cast<QGraphicsOpacityEffect*>(b->graphicsEffect());
    if (!eff) { eff = new QGraphicsOpacityEffect(b); b->setGraphicsEffect(eff); }
    eff->setOpacity(locked ? 0.35 : 1.0);
}

}  // namespace

// ===========================================================================
//  TitleBar
// ===========================================================================
TitleBar::TitleBar(QWidget* parent) : QWidget(parent) {
    setFixedHeight(dim::TitleBarH);

    auto* h = new QHBoxLayout(this);
    h->setContentsMargins(10, 0, 6, 3);   // 3px de folga p/ a faixa tricolor
    h->setSpacing(8);

    // Marca: logo oficial da Soften (cores originais) + "SOFTEN PHONE".
    auto* logo = new QLabel(this);
    QPixmap logoPix(":/assets/logo.png");
    if (!logoPix.isNull())
        logo->setPixmap(logoPix.scaled(20, 20, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    logo->setAttribute(Qt::WA_TransparentForMouseEvents);
    h->addWidget(logo);

    h->addWidget(mkLabel(QStringLiteral("SOFTEN PHONE"), fontPanelTitle(9.5), Qt::white));
    h->addStretch();

    // Chrome: apenas fechar. Nao ha minimizar (esconder e proibido) e a janela
    // nao pode ser arrastada/movida — anti-esconder.
    m_close = new QPushButton(QString(QChar(0xE8BB)), this);
    m_close->setFont(iconPx(10));
    m_close->setFixedSize(30, 26);
    m_close->setCursor(Qt::PointingHandCursor);
    m_close->setStyleSheet(QStringLiteral(
        "QPushButton{border:none;background:transparent;color:#CFEAFB;border-radius:6px;}"
        "QPushButton:hover{background:#E2453D;color:#ffffff;}"));
    connect(m_close, &QPushButton::clicked, this, &TitleBar::closeClicked);
    h->addWidget(m_close);
}

void TitleBar::setLocked(bool locked) {
    m_locked = locked;
    dimButton(m_close, locked);
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
//  TabsBar
// ===========================================================================
TabsBar::TabsBar(QWidget* parent) : QWidget(parent) {
    setFixedHeight(dim::TabsH);

    auto* h = new QHBoxLayout(this);
    h->setContentsMargins(0, 0, 0, 1);   // 1px p/ a divisoria inferior
    h->setSpacing(0);

    const char* names[] = { "Telefone", "Registros", "Config" };
    for (int i = 0; i < 3; ++i) {
        auto* b = new QPushButton(QString::fromUtf8(names[i]), this);
        b->setCursor(Qt::PointingHandCursor);
        b->setFont(fontLabel(9.5));
        b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        connect(b, &QPushButton::clicked, this, [this, i] { emit tabClicked(i); });
        m_btns << b;
        h->addWidget(b, 1);
    }
    setCurrent(0);
}

void TabsBar::setCurrent(int idx) {
    m_current = idx;
    for (int i = 0; i < m_btns.size(); ++i) {
        const bool on = (i == idx);
        m_btns[i]->setStyleSheet(on
            ? QStringLiteral(
                "QPushButton{border:none;border-bottom:2px solid #d4af37;"
                "background:transparent;color:#d4af37;}")
            : QStringLiteral(
                "QPushButton{border:none;border-bottom:2px solid transparent;"
                "background:transparent;color:#7a7b82;}"
                "QPushButton:hover{color:#eef4fb;}"));
    }
}

void TabsBar::setLocked(bool locked) {
    m_locked = locked;
    for (QPushButton* b : m_btns) dimButton(b, locked);
}

void TabsBar::paintEvent(QPaintEvent*) {
    QPainter g(this);
    g.fillRect(rect(), QColor(0x0D, 0x0E, 0x11));
    g.setPen(QPen(QColor(0x1A, 0x1B, 0x1F), 1));
    g.drawLine(0, height() - 1, width(), height() - 1);
}

// ===========================================================================
//  StatusBar
// ===========================================================================
StatusBar::StatusBar(QWidget* parent) : QWidget(parent) {
    setFixedHeight(dim::StatusH);
}

void StatusBar::setStatus(bool ok, const QString& text) {
    m_ok = ok; m_text = text;
    update();
}

void StatusBar::setRamal(const QString& ramal) {
    m_ramal = ramal;
    update();
}

void StatusBar::paintEvent(QPaintEvent*) {
    QPainter g(this);
    g.setRenderHint(QPainter::Antialiasing);
    g.setRenderHint(QPainter::TextAntialiasing);
    g.fillRect(rect(), QColor(0x0D, 0x0E, 0x11));
    g.setPen(QPen(QColor(0x1A, 0x1B, 0x1F), 1));
    g.drawLine(0, 0, width(), 0);

    // Ponto de status + texto (esq).
    const double dot = 7, midY = height() / 2.0;
    const QColor statusCol = m_ok ? QColor(0x33, 0xE0, 0xA0) : QColor(0xF5, 0xB3, 0x01);
    g.setPen(Qt::NoPen);
    g.setBrush(statusCol);
    g.drawEllipse(QRectF(10, midY - dot / 2.0, dot, dot));

    g.setFont(fontLabel(8.5));
    g.setPen(m_ok ? QColor(0xEE, 0xF4, 0xFB) : QColor(0x8A, 0x8B, 0x92));
    const int ramalW = 70;
    g.drawText(QRectF(10 + dot + 7, 0, width() - (10 + dot + 7) - ramalW, height()),
               Qt::AlignVCenter | Qt::AlignLeft, m_text);

    // Ramal (dir), em mono dourado.
    if (!m_ramal.isEmpty()) {
        g.setFont(fontTelemetry(9));
        g.setPen(QColor(0xD4, 0xAF, 0x37));
        g.drawText(QRectF(width() - ramalW - 10, 0, ramalW, height()),
                   Qt::AlignVCenter | Qt::AlignRight, m_ramal);
    }
}

// ===========================================================================
//  DialerPanel
// ===========================================================================
DialerPanel::DialerPanel(QWidget* parent) : QWidget(parent) {
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(dim::PanelPad, 10, dim::PanelPad, dim::PanelPad);
    v->setSpacing(0);

    // Display: card com numero (mono) + backspace.
    auto* card = new QWidget(this);
    card->setObjectName("DialDisplay");
    card->setFixedHeight(46);
    card->setStyleSheet(QStringLiteral(
        "#DialDisplay{background:qlineargradient(x1:0,y1:0,x2:1,y2:0,"
        " stop:0 #1d1e23, stop:1 #17181c);border:1px solid #34353c;border-radius:%1px;}")
        .arg(dim::CardRadius));
    auto* ch = new QHBoxLayout(card);
    ch->setContentsMargins(12, 4, 6, 4);
    m_display = new QLineEdit(card);
    m_display->setFont(fontDisplay(16));
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
    back->setFixedSize(32, 32);
    back->glyph = glyph::Backspace;
    back->glyphSize = 13;
    back->idleGlyph = textSecondary();
    connect(back, &RoundGlyphButton::clicked, this, [this] {
        m_display->backspace();
    });
    ch->addWidget(back, 0);
    v->addWidget(card);
    v->addSpacing(10);

    // Teclado 3x4.
    auto* grid = new QGridLayout();
    grid->setSpacing(8);
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
        key->setMinimumSize(44, 44);
        const QString kc = key->keyChar;
        connect(key, &KeypadButton::clicked, this, [this, kc] {
            if (!kc.isEmpty()) emit keyTone(kc[0]);
            append(kc);
        });
        grid->addWidget(key, i / 3, i % 3);
    }
    v->addLayout(grid, 1);
    v->addSpacing(10);

    // Acao: barra "Chamar" larga (estilo MicroSIP), dourada.
    m_call = new QPushButton(QStringLiteral("Chamar"), this);
    m_call->setCursor(Qt::PointingHandCursor);
    m_call->setFont(fontPanelTitle(11));
    m_call->setFixedHeight(42);
    m_call->setStyleSheet(QStringLiteral(
        "QPushButton{background:qlineargradient(x1:0,y1:0,x2:0,y2:1,"
        " stop:0 #f5d77a, stop:1 #d4af37);border:none;border-radius:%1px;color:#15161a;}"
        "QPushButton:hover{background:qlineargradient(x1:0,y1:0,x2:0,y2:1,"
        " stop:0 #f8e09a, stop:1 #ddba45);}"
        "QPushButton:disabled{background:#26272d;color:#5a5b62;}")
        .arg(dim::CardRadius));
    connect(m_call, &QPushButton::clicked, this, &DialerPanel::callRequested);
    v->addWidget(m_call);
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

void DialerPanel::paintEvent(QPaintEvent*) { paintPageBg(this); }

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

    m_nameLabel  = mkLabel(QString(), fontPanelTitle(14), sig().onField,
                           Qt::AlignHCenter | Qt::AlignVCenter, this);
    m_numberLabel = mkLabel(QString(), fontDisplay(9.5), sig().onFieldSub,
                            Qt::AlignHCenter | Qt::AlignVCenter, this);

    m_timerPill = new QLabel(QStringLiteral("00:00"), this);
    m_timerPill->setAlignment(Qt::AlignCenter);
    m_timerPill->setFont(fontDisplay(11));
    m_timerPill->setStyleSheet(QStringLiteral(
        "color:%1;background:rgba(255,255,255,28);border-radius:11px;padding:2px 10px;")
        .arg(sig().onField.name()));

    // Status da chamada (topo, centrado) — unico cabecalho da pagina.
    auto* tsub = mkLabel(QStringLiteral("EM CURSO"), fontTelemetry(8), sig().cyanLight,
                         Qt::AlignHCenter | Qt::AlignVCenter, this);
    tsub->setObjectName("CallTitleSub");

    // Cluster 1x4 (Active/Held): Mudo | Teclado | Espera | Transferir.
    m_cluster = new QWidget(this);
    auto* cg = new QGridLayout(m_cluster);
    cg->setContentsMargins(0, 0, 0, 0);
    cg->setHorizontalSpacing(12);
    cg->setVerticalSpacing(4);
    auto mkCtrl = [this](const QString& gl, const QString& lab, RoundGlyphButton** out) {
        auto* cell = new QWidget(m_cluster);
        auto* cv = new QVBoxLayout(cell);
        cv->setContentsMargins(0, 0, 0, 0);
        cv->setSpacing(4);
        auto* b = new RoundGlyphButton(cell);
        b->setFixedSize(42, 42);
        b->glyph = gl; b->glyphSize = 16;
        b->idleFill = QColor(255, 255, 255, 18);
        b->idleGlyph = sig().onField;
        b->activeFill = sig().cyan;
        b->activeGlyph = Qt::white;
        cv->addWidget(b, 0, Qt::AlignHCenter);
        cv->addWidget(mkLabel(lab, fontLabel(7.5), sig().onFieldSub,
                              Qt::AlignHCenter | Qt::AlignVCenter, cell));
        if (out) *out = b;
        return cell;
    };
    RoundGlyphButton* keysBtn = nullptr;
    RoundGlyphButton* trfBtn = nullptr;
    auto* cMute = mkCtrl(glyph::Microphone, QStringLiteral("Mudo"), &m_muteBtn);
    auto* cKeys = mkCtrl(glyph::Dialpad,    QStringLiteral("Teclado"), &keysBtn);
    auto* cHold = mkCtrl(glyph::Pause,      QStringLiteral("Espera"), &m_holdBtn);
    auto* cTrf  = mkCtrl(glyph::Transfer,   QStringLiteral("Transf."), &trfBtn);
    cg->addWidget(cMute, 0, 0); cg->addWidget(cKeys, 0, 1);
    cg->addWidget(cHold, 0, 2); cg->addWidget(cTrf, 0, 3);
    // Mudo/Espera NAO sao checkable: o estado ativo e dirigido pela MainWindow
    // (setMuteActive/setHoldActive) apos confirmar com o SIP, evitando duplo-toggle.
    connect(m_muteBtn, &RoundGlyphButton::clicked, this, &CallPanel::muteClicked);
    connect(m_holdBtn, &RoundGlyphButton::clicked, this, &CallPanel::holdClicked);
    connect(keysBtn, &RoundGlyphButton::clicked, this, &CallPanel::keypadClicked);
    connect(trfBtn, &RoundGlyphButton::clicked, this, &CallPanel::transferClicked);

    // Encerrar (vermelho).
    m_hangup = new CallButton(this);
    m_hangup->setFixedSize(52, 52);
    m_hangup->base = sig().red;
    m_hangup->glyph = glyph::Phone;
    m_hangup->glyphRotation = 135;
    connect(m_hangup, &CallButton::clicked, this, &CallPanel::hangupRequested);

    // Incoming (Atender/Recusar).
    m_incoming = new QWidget(this);
    auto* ih = new QHBoxLayout(m_incoming);
    ih->setContentsMargins(0, 0, 0, 0);
    ih->setSpacing(34);
    auto* reject = new CallButton(m_incoming);
    reject->setFixedSize(56, 56);
    reject->base = sig().red; reject->glyph = glyph::Phone; reject->glyphRotation = 135;
    auto* answer = new CallButton(m_incoming);
    answer->setFixedSize(56, 56);
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
void CallPanel::pushAudioLevel(float) { /* waveform removido no shell compacto */ }
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
    m_cluster->setVisible(active);
    m_hangup->setVisible(active || outgoing);
    m_incoming->setVisible(incoming);

    m_rings->setActive(active || outgoing);

    // O status da chamada e o cabecalho da pagina.
    if (auto* sub = findChild<QLabel*>("CallTitleSub")) {
        sub->setVisible(!idle);
        sub->setText(incoming ? QStringLiteral("CHAMADA RECEBIDA")
                   : outgoing ? QStringLiteral("CHAMANDO…")
                   : v == View::Held ? QStringLiteral("EM ESPERA")
                   : QStringLiteral("EM CURSO"));
    }
    relayout();
    // Reposiciona depois do layout pai assentar o tamanho do painel. Sem isto,
    // reconstruir o shell DURANTE a chamada posiciona os botoes com o tamanho
    // antigo/zero e eles "quebram" ate o proximo resize.
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

    // Status no topo, centrado.
    if (auto* s = findChild<QLabel*>("CallTitleSub")) s->setGeometry(0, 8, w, 14);

    if (m_idleHint->isVisible()) {
        m_idleHint->setGeometry(0, h/2 - 20, w, 40);
        return;
    }

    // Pilha vertical centrada: aneis+avatar, nome, numero, timer.
    const int ringSize = std::min(w - 70, 148);
    const int avSize = 62;
    int y = 28;
    m_rings->setGeometry(cx - ringSize/2, y, ringSize, ringSize);
    m_avatar->setGeometry(cx - avSize/2, y + ringSize/2 - avSize/2, avSize, avSize);
    y += ringSize + 4;

    m_nameLabel->setGeometry(10, y, w - 20, 22); y += 24;
    m_numberLabel->setGeometry(10, y, w - 20, 15); y += 19;

    const int pillW = 96;
    m_timerPill->setGeometry(cx - pillW/2, y, pillW, 22);

    // Rodape ancorado embaixo: cluster acima do encerrar (gap garantido); ou
    // incoming (Atender/Recusar); ou so o encerrar (Outgoing).
    if (m_hangup->isVisible())
        m_hangup->setGeometry(cx - 26, h - 62, 52, 52);
    if (m_cluster->isVisible()) {
        const QSize cs = m_cluster->sizeHint();
        m_cluster->setGeometry(cx - cs.width()/2, h - 62 - cs.height() - 10, cs.width(), cs.height());
    }
    if (m_incoming->isVisible())
        m_incoming->setGeometry(0, h - 76, w, 60);
}

// ===========================================================================
//  RecentsPanel
// ===========================================================================
RecentsPanel::RecentsPanel(QWidget* parent) : QWidget(parent) {
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(dim::PanelPad, 10, dim::PanelPad, 8);
    v->setSpacing(0);

    // Busca.
    auto* searchCard = new QWidget(this);
    searchCard->setObjectName("SearchCard");
    searchCard->setFixedHeight(34);
    searchCard->setStyleSheet(QStringLiteral(
        "#SearchCard{background:%1;border:1px solid %2;border-radius:%3px;}")
        .arg(panelGray().name(), border().name()).arg(dim::CardRadius));
    auto* sh = new QHBoxLayout(searchCard);
    sh->setContentsMargins(10, 0, 10, 0);
    sh->setSpacing(6);
    auto* sIcon = mkLabel(glyph::Search, iconPx(12), textTertiary(),
                          Qt::AlignVCenter | Qt::AlignHCenter, searchCard);
    sIcon->setFixedWidth(16);
    sh->addWidget(sIcon);
    m_search = new QLineEdit(searchCard);
    m_search->setFont(fontLabel(9.5));
    m_search->setPlaceholderText(QStringLiteral("Buscar contato ou número"));
    m_search->setStyleSheet(QStringLiteral(
        "QLineEdit{background:transparent;border:none;color:%1;}").arg(textPrimary().name()));
    connect(m_search, &QLineEdit::textChanged, this, [this](const QString& t) {
        if (m_proxy) m_proxy->setSearch(t);
    });
    sh->addWidget(m_search, 1);
    v->addWidget(searchCard);
    v->addSpacing(8);

    // Abas de filtro.
    auto* tabsRow = new QHBoxLayout();
    tabsRow->setSpacing(4);
    m_tabs = new QButtonGroup(this);
    const char* names[] = { "Todas", "Perdidas", "Recebidas", "Feitas" };
    for (int i = 0; i < 4; ++i) {
        auto* b = new QPushButton(QString::fromUtf8(names[i]), this);
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setFont(fontLabel(8));
        b->setStyleSheet(QStringLiteral(
            "QPushButton{border:none;background:transparent;color:%1;padding:4px 7px;border-radius:11px;}"
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
    v->addSpacing(6);

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

    // Rodape de telemetria (alimentado pela MainWindow durante a chamada).
    v->addSpacing(6);
    auto* foot = new QHBoxLayout();
    foot->setSpacing(0);
    auto mkStat = [this](const QString& cap, const QString& val, QLabel** out) {
        auto* cell = new QVBoxLayout();
        cell->setSpacing(1);
        cell->addWidget(mkLabel(cap, fontTelemetry(7), textTertiary()));
        auto* vl = mkLabel(val, fontDisplay(9.5), textSecondary());
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

void RecentsPanel::paintEvent(QPaintEvent*) { paintPageBg(this); }

// ===========================================================================
//  SettingsPanel
// ===========================================================================
SettingsPanel::SettingsPanel(SipConfig* config, QWidget* parent)
    : QWidget(parent), m_config(config) {
    auto* v = new QVBoxLayout(this);
    // Compacto para caber na pagina do shell fixo sem rolagem.
    v->setContentsMargins(dim::PanelPad, 10, dim::PanelPad, 10);
    v->setSpacing(0);

    auto section = [&](const QString& t) {
        auto* l = mkLabel(t.toUpper(), fontTelemetry(7.5), textTertiary());
        v->addWidget(l);
        v->addSpacing(4);
    };
    auto field = [&](const QString& label, bool password, QLineEdit*& out) {
        v->addWidget(mkLabel(label, fontLabel(8.5), textSecondary()));
        v->addSpacing(2);
        out = new QLineEdit(this);
        out->setFont(fontLabel(10));
        out->setFixedHeight(30);
        if (password) out->setEchoMode(QLineEdit::Password);
        out->setStyleSheet(QStringLiteral(
            "QLineEdit{background:%1;border:1px solid %2;border-radius:%3px;padding:0 10px;color:%4;}"
            "QLineEdit:focus{border:1px solid %5;}")
            .arg(panelGray().name(), border().name()).arg(dim::CardRadius)
            .arg(textPrimary().name(), sig().cyan.name()));
        v->addWidget(out);
        v->addSpacing(5);
    };

    auto combo = [&](const QString& label, QComboBox*& out) {
        v->addWidget(mkLabel(label, fontLabel(8.5), textSecondary()));
        v->addSpacing(2);
        out = new QComboBox(this);
        out->setFont(fontLabel(10));
        out->setFixedHeight(30);
        out->setCursor(Qt::PointingHandCursor);
        out->setStyleSheet(QStringLiteral(
            "QComboBox{background:%1;border:1px solid %2;border-radius:%3px;padding:0 10px;color:%4;}"
            "QComboBox:focus{border:1px solid %5;}"
            "QComboBox::drop-down{border:none;width:22px;}"
            "QComboBox QAbstractItemView{background:%1;border:1px solid %2;color:%4;"
            "selection-background-color:%5;outline:none;}")
            .arg(panelGray().name(), border().name()).arg(dim::CardRadius)
            .arg(textPrimary().name(), sig().cyan.name()));
        v->addWidget(out);
        v->addSpacing(5);
    };

    section(QStringLiteral("Conta"));
    field(QStringLiteral("Servidor"), false, m_server);
    field(QStringLiteral("Ramal"), false, m_user);
    field(QStringLiteral("Senha"), true, m_pass);

    v->addSpacing(4);
    section(QStringLiteral("Áudio"));
    combo(QString::fromUtf8("Microfone (falar)"), m_capture);
    combo(QString::fromUtf8("Alto-falante / fone (ouvir)"), m_playback);

    v->addSpacing(4);
    {
        auto* upd = new QPushButton(QString::fromUtf8("Procurar atualização"), this);
        upd->setCursor(Qt::PointingHandCursor);
        upd->setFont(fontLabel(9.5));
        upd->setFixedHeight(30);
        upd->setStyleSheet(QStringLiteral(
            "QPushButton{background:%1;border:1px solid %2;border-radius:%3px;color:%4;}"
            "QPushButton:hover{border:1px solid %5;}")
            .arg(panelGray().name(), border().name()).arg(dim::CardRadius)
            .arg(textPrimary().name(), sig().cyan.name()));
        connect(upd, &QPushButton::clicked, this, &SettingsPanel::checkUpdate);
        v->addWidget(upd);
    }

    v->addStretch();

    // Rodape: versao | Cancelar | Salvar.
    auto* foot = new QHBoxLayout();
    foot->setSpacing(8);
    foot->addWidget(mkLabel(QString::fromUtf8("v") + QStringLiteral(SPHONE_VERSION),
                            fontTelemetry(8), textTertiary()));
    foot->addStretch();
    auto* cancel = new QPushButton(QStringLiteral("Cancelar"), this);
    cancel->setCursor(Qt::PointingHandCursor);
    cancel->setFont(fontLabel(9.5));
    cancel->setFixedSize(78, 30);
    cancel->setStyleSheet(QStringLiteral(
        "QPushButton{background:%1;border:1px solid %2;border-radius:%3px;color:%4;}")
        .arg(panelGray().name(), border().name()).arg(dim::CardRadius).arg(textPrimary().name()));
    connect(cancel, &QPushButton::clicked, this, &SettingsPanel::closed);
    auto* save = new QPushButton(QStringLiteral("Salvar"), this);
    save->setCursor(Qt::PointingHandCursor);
    save->setFont(fontLabel(9.5));
    save->setFixedSize(92, 30);
    save->setStyleSheet(QStringLiteral(
        "QPushButton{background:%1;border:none;border-radius:%2px;color:#15161a;}"
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

void SettingsPanel::paintEvent(QPaintEvent*) { paintPageBg(this); }

}  // namespace sphone
