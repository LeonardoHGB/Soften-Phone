#pragma once
//
// recentsmodel.h — Model/View do historico de chamadas (painel Recentes).
//
// Substitui a montagem manual de N widgets pelo padrao Qt: CallLogModel guarda
// os CallAudit; CallLogProxy filtra por aba (Todas/Perdidas/Recebidas/Feitas) +
// busca; CallLogDelegate pinta cada linha (avatar com iniciais, nome, direcao+
// numero, hora, botao ligar) e emite redial() no clique.
//
#include <QAbstractListModel>
#include <QSortFilterProxyModel>
#include <QStyledItemDelegate>
#include <QList>

#include "data/callaudit.h"

namespace sphone {

class CallLogModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Roles { AuditRole = Qt::UserRole + 1 };
    explicit CallLogModel(QObject* parent = nullptr) : QAbstractListModel(parent) {}

    void setItems(const QList<CallAudit>& items);
    int  rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;

private:
    QList<CallAudit> m_items;
};

class CallLogProxy : public QSortFilterProxyModel {
    Q_OBJECT
public:
    enum class Mode { All, Missed, Incoming, Outgoing };
    explicit CallLogProxy(QObject* parent = nullptr) : QSortFilterProxyModel(parent) {}
    void setMode(Mode m);
    void setSearch(const QString& q);
protected:
    bool filterAcceptsRow(int row, const QModelIndex& parent) const override;
private:
    Mode    m_mode = Mode::All;
    QString m_query;
};

class CallLogDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit CallLogDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}
    void  paint(QPainter*, const QStyleOptionViewItem&, const QModelIndex&) const override;
    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const override;
    bool  editorEvent(QEvent*, QAbstractItemModel*, const QStyleOptionViewItem&, const QModelIndex&) override;
signals:
    void redial(const QString& number);
};

}  // namespace sphone
