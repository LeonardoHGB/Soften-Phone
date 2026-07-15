#include "ui/recentsmodel.h"
#include "core/brand.h"

#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QRegularExpression>
#include <QDate>

using namespace brand;

namespace sphone {

// --- helpers (espelham os do panels.cpp; locais a este TU) ----------------
namespace {

QString initialsFrom(const QString& name, const QString& number) {
    const QString n = name.trimmed();
    if (!n.isEmpty()) {
        const QStringList parts = n.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        if (parts.size() >= 2) return (parts[0].left(1) + parts[1].left(1)).toUpper();
        return parts[0].left(2).toUpper();
    }
    return QString(number).remove(QRegularExpression("[^0-9]")).right(2);
}

QColor avatarColor(const QString& key) {
    static const QColor pool[] = {
        QColor(0x0a, 0x5d, 0xc4), QColor(0x6d, 0x4a, 0xd6), QColor(0x19, 0xa9, 0x6b),
        QColor(0xc9, 0x7a, 0x12), QColor(0x46, 0x9b, 0xd6), QColor(0xb0, 0x3a, 0x4a),
        QColor(0x14, 0xb0, 0xc0), QColor(0x7a, 0x3f, 0xb8), QColor(0x2e, 0x8b, 0x57),
    };
    uint h = 0;
    for (QChar c : key) h = h * 31 + c.unicode();
    return pool[h % (sizeof(pool) / sizeof(pool[0]))];
}

QString formatWhen(const QDateTime& when) {
    const QDate today = QDate::currentDate();
    if (when.date() == today)             return when.toString("HH:mm");
    if (when.date() == today.addDays(-1)) return QStringLiteral("ontem");
    return when.toString("dd/MM");
}

constexpr int kRowH = 56;

// Retangulo do botao "ligar" (circulo a direita da linha).
QRectF callBtnRect(const QRect& row) {
    const double d = 32;
    return QRectF(row.right() - 8 - d, row.center().y() - d / 2.0, d, d);
}

bool isMissed(const CallAudit& c) {
    return c.direction == CallDirection::Inbound && !c.answered && !c.answeredElsewhere;
}

}  // namespace

// ===========================================================================
//  CallLogModel
// ===========================================================================
void CallLogModel::setItems(const QList<CallAudit>& items) {
    beginResetModel();
    m_items = items;
    endResetModel();
}
int CallLogModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : int(m_items.size());
}
QVariant CallLogModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size()) return {};
    if (role == AuditRole) return QVariant::fromValue(m_items[index.row()]);
    if (role == Qt::DisplayRole) {   // util p/ acessibilidade/busca padrao
        const CallAudit& c = m_items[index.row()];
        return c.peerName.isEmpty() ? c.peerNumber : c.peerName;
    }
    return {};
}

// ===========================================================================
//  CallLogProxy
// ===========================================================================
void CallLogProxy::setMode(Mode m)  { m_mode = m; invalidateFilter(); }
void CallLogProxy::setSearch(const QString& q) { m_query = q.trimmed(); invalidateFilter(); }

bool CallLogProxy::filterAcceptsRow(int row, const QModelIndex& parent) const {
    const QModelIndex idx = sourceModel()->index(row, 0, parent);
    const CallAudit c = idx.data(CallLogModel::AuditRole).value<CallAudit>();
    const bool inbound = c.direction == CallDirection::Inbound;

    switch (m_mode) {
        case Mode::Missed:   if (!isMissed(c)) return false; break;
        case Mode::Incoming: if (!inbound)     return false; break;
        case Mode::Outgoing: if (inbound)      return false; break;
        case Mode::All: break;
    }
    if (!m_query.isEmpty() &&
        !c.peerName.contains(m_query, Qt::CaseInsensitive) &&
        !c.peerNumber.contains(m_query, Qt::CaseInsensitive))
        return false;
    return true;
}

// ===========================================================================
//  CallLogDelegate
// ===========================================================================
QSize CallLogDelegate::sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const {
    return QSize(0, kRowH);
}

void CallLogDelegate::paint(QPainter* p, const QStyleOptionViewItem& opt, const QModelIndex& index) const {
    const CallAudit c = index.data(CallLogModel::AuditRole).value<CallAudit>();
    const bool inbound = c.direction == CallDirection::Inbound;
    const bool missed = isMissed(c);
    const QRect r = opt.rect;

    p->save();
    p->setRenderHint(QPainter::Antialiasing);
    p->setRenderHint(QPainter::TextAntialiasing);

    // Fundo (hover).
    if (opt.state & QStyle::State_MouseOver) {
        QPainterPath bg; bg.addRoundedRect(QRectF(r.adjusted(2, 2, -2, -2)), 12, 12);
        p->fillPath(bg, panelGray());
    }

    const QString title = !c.peerName.trimmed().isEmpty() ? c.peerName
                        : (c.peerNumber.trimmed().isEmpty() ? QStringLiteral("Desconhecido") : c.peerNumber);

    // Avatar (circulo + iniciais).
    const double av = 34, ax = r.left() + 8, ay = r.center().y() - av / 2.0;
    QRectF avRect(ax, ay, av, av);
    p->setPen(Qt::NoPen);
    p->setBrush(avatarColor(title));
    p->drawEllipse(avRect);
    p->setFont(fontPanelTitle(10));
    p->setPen(Qt::white);
    p->drawText(avRect, Qt::AlignCenter, initialsFrom(c.peerName, c.peerNumber));

    // Colunas da linha estreita: [avatar | titulo/sub | hora | botao].
    const QRectF cb = callBtnRect(r);
    const double tx = ax + av + 10;
    const QRectF timeRect(cb.left() - 6 - 40, r.top() + 10, 40, 14);
    const double titleW = timeRect.left() - 4 - tx;
    const double subW   = cb.left() - 6 - tx;

    // Nome (com elipse: a coluna e curta).
    QFont tf = fontLabel(10.5);
    p->setFont(tf);
    p->setPen(textPrimary());
    const QString elided = QFontMetrics(tf).elidedText(title, Qt::ElideRight, int(titleW));
    p->drawText(QRectF(tx, r.top() + 10, titleW, 18), Qt::AlignVCenter | Qt::AlignLeft, elided);

    // Direcao (seta) + numero/desfecho.
    const QString arrow = inbound ? QStringLiteral("↙") : QStringLiteral("↗");
    const QString sub = QStringLiteral("%1  %2").arg(arrow, c.peerNumber.isEmpty() ? c.outcome : c.peerNumber);
    QFont sf = fontLabel(8.5);
    p->setFont(sf);
    p->setPen(missed ? sig().red : textSecondary());
    p->drawText(QRectF(tx, r.top() + 29, subW, 16), Qt::AlignVCenter | Qt::AlignLeft,
                QFontMetrics(sf).elidedText(sub, Qt::ElideRight, int(subW)));

    // Hora (coluna propria, antes do botao — nao invade o titulo).
    p->setFont(fontTelemetry(7.5));
    p->setPen(textTertiary());
    p->drawText(timeRect, Qt::AlignRight | Qt::AlignVCenter, formatWhen(c.startedLocal));

    // Botao ligar.
    p->setPen(Qt::NoPen);
    p->setBrush(blend(sig().cyan, bodyBg(), 0.84));
    p->drawEllipse(cb);
    p->setFont(iconPx(13));
    p->setPen(sig().cyan);
    p->drawText(cb, Qt::AlignCenter, glyph::Phone);

    p->restore();
}

bool CallLogDelegate::editorEvent(QEvent* e, QAbstractItemModel*,
                                  const QStyleOptionViewItem&, const QModelIndex& index) {
    if (e->type() == QEvent::MouseButtonRelease) {
        const CallAudit c = index.data(CallLogModel::AuditRole).value<CallAudit>();
        if (!c.peerNumber.trimmed().isEmpty()) { emit redial(c.peerNumber); return true; }
    }
    return false;
}

}  // namespace sphone
