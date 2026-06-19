#include "ui/controls.h"
#include "core/brand.h"

#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QEnterEvent>
#include <QFontMetricsF>
#include <algorithm>
#include <cmath>

using namespace brand;

namespace sphone {

// Centraliza um glifo na origem atual do painter (apos translate/rotate).
static void drawGlyphCentered(QPainter& g, const QString& glyph) {
    g.drawText(QRectF(-400, -400, 800, 800), Qt::AlignCenter, glyph);
}

static QString spaceOut(const QString& s) {
    QString out;
    for (int i = 0; i < s.size(); ++i) { if (i) out += ' '; out += s[i]; }
    return out;
}

// ---------------------------------------------------------------------------
// ClickableWidget
// ---------------------------------------------------------------------------
ClickableWidget::ClickableWidget(QWidget* parent) : QWidget(parent) {
    setCursor(Qt::PointingHandCursor);
}
void ClickableWidget::mouseReleaseEvent(QMouseEvent* e) {
    if (rect().contains(e->pos())) emit clicked();
}

// ---------------------------------------------------------------------------
// RoundedCard
// ---------------------------------------------------------------------------
RoundedCard::RoundedCard(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_TranslucentBackground);
}

void RoundedCard::paintEvent(QPaintEvent*) {
    QPainter g(this);
    g.setRenderHint(QPainter::Antialiasing);
    QRectF r(0, 0, width(), height());
    if (borderThickness > 0) {
        const double t = borderThickness / 2.0 + 0.5;
        r.adjust(t, t, -t, -t);
    }
    QPainterPath path;
    path.addRoundedRect(r, radius, radius);
    g.fillPath(path, fillColor);
    if (borderThickness > 0)
        g.strokePath(path, QPen(borderColor, borderThickness));
}

// ---------------------------------------------------------------------------
// IconButton
// ---------------------------------------------------------------------------
IconButton::IconButton(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_TranslucentBackground);
    setCursor(Qt::PointingHandCursor);
}
void IconButton::enterEvent(QEnterEvent*)       { m_hover = true; update(); }
void IconButton::leaveEvent(QEvent*)            { m_hover = false; m_down = false; update(); }
void IconButton::mousePressEvent(QMouseEvent*)  { m_down = true; update(); }
void IconButton::mouseReleaseEvent(QMouseEvent* e) {
    m_down = false; update();
    if (rect().contains(e->pos())) emit clicked();
}
void IconButton::paintEvent(QPaintEvent*) {
    QPainter g(this);
    g.setRenderHint(QPainter::Antialiasing);
    g.setRenderHint(QPainter::TextAntialiasing);

    const double pad = m_down ? 3.0 : 1.5;
    QRectF r(pad, pad, width() - pad * 2, height() - pad * 2);
    QColor f = m_down  ? blend(fill, Qt::black, 0.12)
             : m_hover ? blend(fill, Qt::white, 0.08) : fill;
    g.setPen(Qt::NoPen);
    g.setBrush(f);
    g.drawEllipse(r);

    g.save();
    g.translate(width() / 2.0, height() / 2.0);
    if (glyphRotation != 0) g.rotate(glyphRotation);
    g.setFont(iconPx(int(glyphSize)));
    g.setPen(glyphColor);
    drawGlyphCentered(g, glyph);
    g.restore();
}

// ---------------------------------------------------------------------------
// ActionButton
// ---------------------------------------------------------------------------
ActionButton::ActionButton(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_TranslucentBackground);
    setCursor(Qt::PointingHandCursor);
    fill = Cyan;
}
void ActionButton::enterEvent(QEnterEvent*)      { m_hover = true; update(); }
void ActionButton::leaveEvent(QEvent*)           { m_hover = false; m_down = false; update(); }
void ActionButton::mousePressEvent(QMouseEvent*) { m_down = true; update(); }
void ActionButton::mouseReleaseEvent(QMouseEvent* e) {
    m_down = false; update();
    if (rect().contains(e->pos())) emit clicked();
}
void ActionButton::paintEvent(QPaintEvent*) {
    QPainter g(this);
    g.setRenderHint(QPainter::Antialiasing);
    g.setRenderHint(QPainter::TextAntialiasing);

    QColor f = !isEnabled() ? blend(fill, Qt::white, 0.45)
             : m_down       ? blend(fill, Qt::black, 0.12)
             : m_hover      ? blend(fill, Qt::white, 0.06) : fill;
    QPainterPath path;
    path.addRoundedRect(QRectF(0, 0, width(), height()), radius, radius);
    g.fillPath(path, f);

    const bool hasGlyph = !glyph.isEmpty();
    const QFont gfont = iconPx(int(glyphSize));
    const QFont tfont = semiPt(11.5);
    const double textW = QFontMetricsF(tfont).horizontalAdvance(m_text);
    const double glyphW = hasGlyph ? glyphSize + 2 : 0;
    const double gap = (hasGlyph && !m_text.isEmpty()) ? 9 : 0;
    const double total = glyphW + gap + textW;
    const double startX = (width() - total) / 2.0;
    const double midY = height() / 2.0;

    g.setPen(textColor);
    if (hasGlyph) {
        g.save();
        g.translate(startX + glyphW / 2.0, midY);
        if (glyphRotation != 0) g.rotate(glyphRotation);
        g.setFont(gfont);
        drawGlyphCentered(g, glyph);
        g.restore();
    }
    if (!m_text.isEmpty()) {
        g.setFont(tfont);
        g.drawText(QRectF(startX + glyphW + gap, -1, width(), height()),
                   Qt::AlignVCenter | Qt::AlignLeft, m_text);
    }
}

// ---------------------------------------------------------------------------
// DialKey
// ---------------------------------------------------------------------------
DialKey::DialKey(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_TranslucentBackground);
    setCursor(Qt::PointingHandCursor);
}
void DialKey::enterEvent(QEnterEvent*)       { m_hover = true; update(); }
void DialKey::leaveEvent(QEvent*)            { m_hover = false; m_down = false; update(); }
void DialKey::mousePressEvent(QMouseEvent*)  { m_down = true; update(); }
void DialKey::mouseReleaseEvent(QMouseEvent* e) {
    m_down = false; update();
    if (rect().contains(e->pos())) emit clicked();
}
void DialKey::paintEvent(QPaintEvent*) {
    QPainter g(this);
    g.setRenderHint(QPainter::Antialiasing);
    g.setRenderHint(QPainter::TextAntialiasing);

    QRectF r(0.5, 0.5, width() - 1, height() - 1);
    QColor fill = m_down  ? blend(panelGray(), Qt::black, 0.05)
                : m_hover ? panelGray() : bodyBg();
    QPainterPath path;
    path.addRoundedRect(r, 10, 10);
    g.fillPath(path, fill);
    g.strokePath(path, QPen(border(), 1));

    const double w = width(), h = height();
    g.setPen(textPrimary());
    g.setFont(uiPx(17));
    if (!letters.isEmpty()) {
        g.drawText(QRectF(0, h * 0.10, w, h * 0.55), Qt::AlignCenter, keyChar);
        g.setFont(uiPx(8));
        g.setPen(textTertiary());
        g.drawText(QRectF(0, h * 0.58, w, h * 0.32), Qt::AlignCenter, spaceOut(letters));
    } else {
        g.drawText(QRectF(0, 0, w, h), Qt::AlignCenter, keyChar);
    }
}

// ---------------------------------------------------------------------------
// CallControl
// ---------------------------------------------------------------------------
CallControl::CallControl(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_TranslucentBackground);
    setCursor(Qt::PointingHandCursor);
}
void CallControl::enterEvent(QEnterEvent*) { m_hover = true; update(); }
void CallControl::leaveEvent(QEvent*)      { m_hover = false; update(); }
void CallControl::mouseReleaseEvent(QMouseEvent* e) {
    if (rect().contains(e->pos())) emit clicked();
}
void CallControl::paintEvent(QPaintEvent*) {
    QPainter g(this);
    g.setRenderHint(QPainter::Antialiasing);
    g.setRenderHint(QPainter::TextAntialiasing);

    const double w = width(), h = height();
    QRectF r(0.5, 0.5, w - 1, h - 1);
    if (m_active || m_hover) {
        QPainterPath path;
        path.addRoundedRect(r, 10, 10);
        g.fillPath(path, m_active ? blend(Navy, Qt::white, 0.86) : panelGray());
    }
    g.setFont(iconPx(22));
    g.setPen(Navy);
    g.drawText(QRectF(0, h * 0.12, w, h * 0.50), Qt::AlignCenter, glyph);

    g.setFont(uiPx(10));
    g.setPen(textSecondary());
    g.drawText(QRectF(0, h * 0.62, w, h * 0.32), Qt::AlignCenter, label);
}

// ---------------------------------------------------------------------------
// ToggleSwitch
// ---------------------------------------------------------------------------
ToggleSwitch::ToggleSwitch(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_TranslucentBackground);
    setCursor(Qt::PointingHandCursor);
    setFixedSize(46, 26);
}
void ToggleSwitch::mouseReleaseEvent(QMouseEvent* e) {
    if (rect().contains(e->pos())) setChecked(!m_checked);
}
void ToggleSwitch::paintEvent(QPaintEvent*) {
    QPainter g(this);
    g.setRenderHint(QPainter::Antialiasing);

    const double w = width(), h = height();
    QRectF track(0, (h - 24) / 2.0, w, 24);
    QPainterPath path;
    path.addRoundedRect(track, 12, 12);
    g.fillPath(path, m_checked ? Green : ToggleTrackOff);

    const double d = 18;
    const double ky = (h - d) / 2.0;
    const double kx = m_checked ? w - d - 4 : 4;
    g.setPen(Qt::NoPen);
    g.setBrush(Qt::white);
    g.drawEllipse(QRectF(kx, ky, d, d));
}

// ---------------------------------------------------------------------------
// StatusIndicator
// ---------------------------------------------------------------------------
StatusIndicator::StatusIndicator(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_TranslucentBackground);
}
void StatusIndicator::paintEvent(QPaintEvent*) {
    QPainter g(this);
    g.setRenderHint(QPainter::Antialiasing);
    g.setRenderHint(QPainter::TextAntialiasing);

    const double dot = 11, gap = 8;
    g.setFont(font());
    const double textW = QFontMetricsF(font()).horizontalAdvance(m_text);
    const double total = dot + gap + textW;
    const double startX = std::max(2.0, (width() - total) / 2.0);
    const double midY = height() / 2.0;

    QColor dotColor = m_level == StatusLevel::Error ? Red
                    : m_level == StatusLevel::Warn  ? Amber : Green;
    g.setPen(Qt::NoPen);
    g.setBrush(dotColor);
    g.drawEllipse(QRectF(startX, midY - dot / 2.0, dot, dot));

    g.setPen(foreground);
    g.drawText(QRectF(startX + dot + gap, 0, width(), height()),
               Qt::AlignVCenter | Qt::AlignLeft, m_text);
}

// ---------------------------------------------------------------------------
// LevelBar
// ---------------------------------------------------------------------------
LevelBar::LevelBar(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_TranslucentBackground);
}
void LevelBar::paintEvent(QPaintEvent*) {
    QPainter g(this);
    g.setRenderHint(QPainter::Antialiasing);
    g.setRenderHint(QPainter::TextAntialiasing);

    const double h = height();
    g.setFont(iconPx(14));
    g.setPen(Navy);
    g.drawText(QRectF(0, 0, 22, h), Qt::AlignCenter, glyph);

    const double capW = 76;
    g.setFont(uiPt(9));
    g.setPen(textSecondary());
    g.drawText(QRectF(24, 0, capW, h), Qt::AlignVCenter | Qt::AlignLeft, caption);

    const double mx = 24 + capW + 6;
    const double mw = width() - mx - 2;
    if (mw < 10) return;
    const int segs = 22;
    const double gap = 2;
    const double segW = (mw - (segs - 1) * gap) / segs;
    const double mh = 10;
    const double my = (h - mh) / 2.0;
    const int lit = int(std::lround(std::clamp(m_level, 0.0f, 1.0f) * segs));

    g.setPen(Qt::NoPen);
    for (int i = 0; i < segs; ++i) {
        const double x = mx + i * (segW + gap);
        QColor c;
        if (i < lit) {
            const double frac = double(i) / segs;
            c = frac < 0.6 ? Green : frac < 0.85 ? VuYellow : Red;
        } else {
            c = border();
        }
        g.fillRect(QRectF(x, my, segW, mh), c);
    }
}

// ---------------------------------------------------------------------------
// Avatar
// ---------------------------------------------------------------------------
Avatar::Avatar(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_TranslucentBackground);
}
void Avatar::paintEvent(QPaintEvent*) {
    QPainter g(this);
    g.setRenderHint(QPainter::Antialiasing);
    g.setRenderHint(QPainter::TextAntialiasing);

    QRectF r(1, 1, width() - 2, height() - 2);
    g.setPen(Qt::NoPen);
    g.setBrush(AvatarFill);
    g.drawEllipse(r);
    g.setBrush(Qt::NoBrush);
    g.setPen(QPen(AvatarBorder, 2));
    g.drawEllipse(r);

    g.setFont(iconPx(int(glyphSize)));
    g.setPen(Qt::white);
    g.drawText(QRectF(0, 0, width(), height()), Qt::AlignCenter, glyph::Contact);
}

}  // namespace sphone
