#include "ui/signalwidgets.h"
#include "core/brand.h"

#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QEnterEvent>
#include <QFontMetricsF>
#include <QRadialGradient>
#include <QLinearGradient>
#include <QVariantAnimation>
#include <QEasingCurve>
#include <QtMath>
#include <algorithm>
#include <cmath>

using namespace brand;

namespace sphone {

static QString spaceOut(const QString& s) {
    QString out;
    for (int i = 0; i < s.size(); ++i) { if (i) out += ' '; out += s[i]; }
    return out;
}

// Maior circulo inscrito e centrado no widget.
static QRectF inscribedCircle(const QRectF& r) {
    const double d = std::min(r.width(), r.height());
    return QRectF(r.center().x() - d / 2.0, r.center().y() - d / 2.0, d, d);
}

// ---------------------------------------------------------------------------
// KeypadButton
// ---------------------------------------------------------------------------
KeypadButton::KeypadButton(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_TranslucentBackground);
    setCursor(Qt::PointingHandCursor);
}
void KeypadButton::enterEvent(QEnterEvent*)      { m_hover = true; update(); }
void KeypadButton::leaveEvent(QEvent*)           { m_hover = false; m_down = false; update(); }
void KeypadButton::mousePressEvent(QMouseEvent*) { m_down = true; update(); }
void KeypadButton::mouseReleaseEvent(QMouseEvent* e) {
    m_down = false; update();
    if (rect().contains(e->pos())) emit clicked();
}
void KeypadButton::paintEvent(QPaintEvent*) {
    QPainter g(this);
    g.setRenderHint(QPainter::Antialiasing);
    g.setRenderHint(QPainter::TextAntialiasing);

    QRectF c = inscribedCircle(QRectF(0, 0, width(), height()));
    c.adjust(1, 1, -1, -1);

    QColor fill = m_down  ? blend(panelGray(), sig().cyan, 0.10)
                : m_hover ? blend(panelGray(), Qt::white, isDark() ? 0.06 : 0.30)
                          : panelGray();
    g.setPen(QPen(border(), 1.0));
    g.setBrush(fill);
    g.drawEllipse(c);

    const double w = width(), h = height();
    if (!letters.isEmpty()) {
        g.setPen(textPrimary());
        QFont fd = fontPanelTitle(h * 0.205);
        g.setFont(fd);
        g.drawText(QRectF(0, h * 0.16, w, h * 0.46), Qt::AlignCenter, keyChar);
        g.setPen(textTertiary());
        g.setFont(fontTelemetry(h * 0.075));
        g.drawText(QRectF(0, h * 0.60, w, h * 0.24), Qt::AlignCenter, spaceOut(letters));
    } else {
        g.setPen(textPrimary());
        g.setFont(fontPanelTitle(h * 0.24));
        g.drawText(QRectF(0, 0, w, h), Qt::AlignCenter, keyChar);
    }
}

// ---------------------------------------------------------------------------
// CallButton
// ---------------------------------------------------------------------------
CallButton::CallButton(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_TranslucentBackground);
    setCursor(Qt::PointingHandCursor);
    base = sig().cyan;
}
void CallButton::enterEvent(QEnterEvent*)      { m_hover = true; update(); }
void CallButton::leaveEvent(QEvent*)           { m_hover = false; m_down = false; update(); }
void CallButton::mousePressEvent(QMouseEvent*) { m_down = true; update(); }
void CallButton::mouseReleaseEvent(QMouseEvent* e) {
    m_down = false; update();
    if (rect().contains(e->pos())) emit clicked();
}
void CallButton::paintEvent(QPaintEvent*) {
    QPainter g(this);
    g.setRenderHint(QPainter::Antialiasing);
    g.setRenderHint(QPainter::TextAntialiasing);

    QRectF full = inscribedCircle(QRectF(0, 0, width(), height()));
    // Halo externo suave.
    if (glow) {
        const double pad = full.width() * 0.10;
        QRectF halo = full.adjusted(-pad, -pad, pad, pad);
        QColor h = base; h.setAlpha(46);
        g.setPen(Qt::NoPen);
        g.setBrush(h);
        g.drawEllipse(halo.adjusted(halo.width()*0.04, halo.width()*0.04, -halo.width()*0.04, -halo.width()*0.04));
    }

    QColor lo = base;
    QColor hi = blend(base, Qt::white, 0.28);
    if (!isEnabled()) { lo = blend(lo, Qt::white, 0.45); hi = blend(hi, Qt::white, 0.45); }
    else if (m_down)  { lo = blend(lo, Qt::black, 0.12); hi = blend(hi, Qt::black, 0.12); }
    else if (m_hover) { lo = blend(lo, Qt::white, 0.06); hi = blend(hi, Qt::white, 0.06); }

    QRectF body = full.adjusted(full.width()*0.10, full.width()*0.10,
                                -full.width()*0.10, -full.width()*0.10);
    QLinearGradient grad(body.topLeft(), body.bottomLeft());
    grad.setColorAt(0.0, hi);
    grad.setColorAt(1.0, lo);
    g.setPen(Qt::NoPen);
    g.setBrush(grad);
    g.drawEllipse(body);

    g.save();
    g.translate(body.center());
    if (glyphRotation != 0) g.rotate(glyphRotation);
    g.setFont(iconPx(int(glyphSize)));
    g.setPen(Qt::white);
    g.drawText(QRectF(-400, -400, 800, 800), Qt::AlignCenter, glyph.isEmpty() ? glyph::Phone : glyph);
    g.restore();
}

// ---------------------------------------------------------------------------
// RoundGlyphButton
// ---------------------------------------------------------------------------
RoundGlyphButton::RoundGlyphButton(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_TranslucentBackground);
    setCursor(Qt::PointingHandCursor);
    activeFill = sig().cyan;
}
void RoundGlyphButton::enterEvent(QEnterEvent*)      { m_hover = true; update(); }
void RoundGlyphButton::leaveEvent(QEvent*)           { m_hover = false; m_down = false; update(); }
void RoundGlyphButton::mousePressEvent(QMouseEvent*) { m_down = true; update(); }
void RoundGlyphButton::mouseReleaseEvent(QMouseEvent* e) {
    m_down = false; update();
    if (!rect().contains(e->pos())) return;
    if (checkable) { m_active = !m_active; update(); emit toggled(m_active); }
    emit clicked();
}
void RoundGlyphButton::paintEvent(QPaintEvent*) {
    QPainter g(this);
    g.setRenderHint(QPainter::Antialiasing);
    g.setRenderHint(QPainter::TextAntialiasing);

    QColor fill   = m_active ? activeFill : idleFill;
    QColor glyphC = m_active ? activeGlyph : idleGlyph;
    if (m_hover && !m_active) fill = (fill.alpha() == 0)
        ? QColor(255, 255, 255, isDark() ? 14 : 22)
        : blend(fill, Qt::white, 0.06);
    if (m_down) fill = blend(fill.alpha() == 0 ? idleGlyph : fill, Qt::black, 0.10);

    QPainterPath path;
    QRectF r;
    if (shape == Shape::Circle) {
        r = inscribedCircle(QRectF(0, 0, width(), height())).adjusted(1, 1, -1, -1);
        path.addEllipse(r);
    } else {
        r = QRectF(1, 1, width() - 2, height() - 2);
        path.addRoundedRect(r, squareRadius, squareRadius);
    }
    if (fill.alpha() > 0) g.fillPath(path, fill);
    if (idleBorder.alpha() > 0 && !m_active) g.strokePath(path, QPen(idleBorder, 1.2));

    g.save();
    g.translate(r.center());
    if (glyphRotation != 0) g.rotate(glyphRotation);
    g.setFont(iconPx(int(glyphSize)));
    g.setPen(glyphC);
    g.drawText(QRectF(-400, -400, 800, 800), Qt::AlignCenter, glyph);
    g.restore();
}

// ---------------------------------------------------------------------------
// RingAvatar
// ---------------------------------------------------------------------------
RingAvatar::RingAvatar(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_TranslucentBackground);
    base = sig().cyan;
}
void RingAvatar::paintEvent(QPaintEvent*) {
    QPainter g(this);
    g.setRenderHint(QPainter::Antialiasing);
    g.setRenderHint(QPainter::TextAntialiasing);

    QRectF c = inscribedCircle(QRectF(0, 0, width(), height())).adjusted(1, 1, -1, -1);
    g.setPen(Qt::NoPen);
    if (gradient) {
        QRadialGradient rg(c.center() - QPointF(0, c.height() * 0.12), c.width() * 0.72);
        rg.setColorAt(0.0, blend(base, Qt::white, 0.34));
        rg.setColorAt(1.0, blend(base, Qt::black, 0.22));
        g.setBrush(rg);
    } else {
        g.setBrush(base);
    }
    g.drawEllipse(c);

    if (!initials.isEmpty()) {
        const double pt = textPt > 0 ? textPt : c.height() * 0.30;
        g.setFont(fontPanelTitle(pt));
        g.setPen(Qt::white);
        g.drawText(c, Qt::AlignCenter, initials.left(2).toUpper());
    } else {
        g.setFont(iconPx(int(c.height() * 0.40)));
        g.setPen(Qt::white);
        g.drawText(c, Qt::AlignCenter, glyph::Contact);
    }
}

// ---------------------------------------------------------------------------
// SignalRings  (fase 1: estatico)
// ---------------------------------------------------------------------------
SignalRings::SignalRings(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_TransparentForMouseEvents);
    color = sig().cyan;
    m_anim = new QVariantAnimation(this);
    m_anim->setStartValue(0.0);
    m_anim->setEndValue(1.0);
    m_anim->setDuration(2400);
    m_anim->setLoopCount(-1);
    m_anim->setEasingCurve(QEasingCurve::InOutSine);
    connect(m_anim, &QVariantAnimation::valueChanged, this,
            [this](const QVariant& v) { setPhase(v.toReal()); });
}

void SignalRings::setActive(bool on) {
    if (m_active == on) return;
    m_active = on;
    if (on) m_anim->start();
    else { m_anim->stop(); m_phase = 0.20; }
    update();
}
void SignalRings::paintEvent(QPaintEvent*) {
    QPainter g(this);
    g.setRenderHint(QPainter::Antialiasing);
    const QPointF c = rect().center();
    const double unit = std::min(width(), height());
    const std::array<double, 3> frac = {0.30, 0.40, 0.50};

    int i = 0;
    for (double f : frac) {
        const double r = unit * f;
        QPen pen(color);
        pen.setWidthF(1.3);
        pen.setDashPattern({2, 8});
        const double op = 0.12 + i * 0.05 + 0.08 * std::sin(m_phase * 2 * M_PI);
        g.setOpacity(std::clamp(op, 0.0, 0.45));
        g.setPen(pen);
        g.setBrush(Qt::NoBrush);
        g.drawEllipse(c, r, r);
        ++i;
    }
    // Dois arcos de acento (quebram a simetria, como no mockup).
    QPen apen(color); apen.setWidthF(1.6); apen.setCapStyle(Qt::RoundCap);
    g.setPen(apen); g.setOpacity(0.5);
    const double ra = unit * 0.40;
    QRectF arcRect(c.x() - ra, c.y() - ra, 2 * ra, 2 * ra);
    g.drawArc(arcRect, 40 * 16, 55 * 16);
    g.drawArc(arcRect, 220 * 16, 45 * 16);
}

// ---------------------------------------------------------------------------
// Waveform  (fase 1: envelope estatico determinístico)
// ---------------------------------------------------------------------------
Waveform::Waveform(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_TransparentForMouseEvents);
    color = sig().cyan;
    // Envelope fixo (pseudo) só para dar a forma; a fase 2 sobrescreve via pushLevel.
    for (size_t i = 0; i < m_bars.size(); ++i) {
        const double t = double(i) / m_bars.size();
        const double env = std::sin(t * M_PI);                 // cresce e decresce
        const double ripple = 0.55 + 0.45 * std::sin(i * 0.9); // textura
        m_bars[i] = float(std::clamp(env * ripple, 0.06, 1.0));
    }
}
void Waveform::pushLevel(float rx) {
    m_bars[m_head] = std::clamp(rx, 0.0f, 1.0f);
    m_head = (m_head + 1) % int(m_bars.size());
    update();
}
void Waveform::paintEvent(QPaintEvent*) {
    QPainter g(this);
    g.setRenderHint(QPainter::Antialiasing);
    const double w = width(), h = height();
    const int n = int(m_bars.size());
    const double gap = 2.0;
    const double bw = std::max(1.5, (w - (n - 1) * gap) / n);
    const double midY = h / 2.0;
    g.setPen(Qt::NoPen);
    g.setOpacity(m_paused ? 0.35 : 1.0);
    for (int i = 0; i < n; ++i) {
        const double v = m_bars[i];
        const double bh = std::max(2.0, v * (h - 4));
        const double x = i * (bw + gap);
        QColor col = blend(color, Qt::white, 0.10 + 0.25 * v);
        g.setBrush(col);
        g.drawRoundedRect(QRectF(x, midY - bh / 2.0, bw, bh), bw / 2.0, bw / 2.0);
    }
}

// ---------------------------------------------------------------------------
// RegPill
// ---------------------------------------------------------------------------
RegPill::RegPill(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_TranslucentBackground);
    setFixedHeight(26);
}
void RegPill::setRegistered(bool ok, const QString& text) {
    m_ok = ok; m_text = text;
    updateGeometry(); update();
}
QSize RegPill::sizeHint() const {
    QFontMetricsF fm(fontTelemetry(8));
    return QSize(int(fm.horizontalAdvance(m_text) + 40), 26);
}
void RegPill::paintEvent(QPaintEvent*) {
    QPainter g(this);
    g.setRenderHint(QPainter::Antialiasing);
    g.setRenderHint(QPainter::TextAntialiasing);

    QRectF r(0.5, (height() - 24) / 2.0, width() - 1, 24);
    QPainterPath path; path.addRoundedRect(r, 12, 12);
    g.fillPath(path, QColor(255, 255, 255, 28));
    g.strokePath(path, QPen(QColor(255, 255, 255, 40), 1));

    const double dot = 8, dx = 14, midY = height() / 2.0;
    g.setPen(Qt::NoPen);
    g.setBrush(m_ok ? sig().green : sig().red);
    g.drawEllipse(QRectF(dx, midY - dot / 2.0, dot, dot));

    g.setFont(fontTelemetry(8));
    g.setPen(QColor(0xCF, 0xEA, 0xFB));
    g.drawText(QRectF(dx + dot + 7, 0, width() - (dx + dot + 7) - 12, height()),
               Qt::AlignVCenter | Qt::AlignLeft, m_text);
}

}  // namespace sphone
