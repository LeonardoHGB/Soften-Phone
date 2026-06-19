#pragma once
//
// promptdialog.h — Dialogo de entrada reutilizavel (porte de PromptDialog.cs).
// Usado para transferir chamada (digitsOnly) e para a senha de supervisor
// (password). 340x196, frameless com cabecalho Navy.
//
#include <QDialog>
#include <QString>

class QLineEdit;

namespace sphone {

class PromptDialog : public QDialog {
    Q_OBJECT
public:
    PromptDialog(const QString& title, const QString& label,
                 const QString& placeholder = QString(),
                 bool password = false, bool digitsOnly = false,
                 const QString& okText = QStringLiteral("OK"),
                 QWidget* parent = nullptr);

    QString value() const;

private:
    QLineEdit* m_edit = nullptr;
};

}  // namespace sphone
