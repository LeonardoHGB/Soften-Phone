#include "ui/promptdialog.h"
#include "ui/controls.h"
#include "core/brand.h"

#include <QLabel>
#include <QLineEdit>
#include <QRegularExpressionValidator>
#include <QPainter>

using namespace brand;

namespace sphone {

PromptDialog::PromptDialog(const QString& title, const QString& label,
                           const QString& placeholder, bool password, bool digitsOnly,
                           const QString& okText, QWidget* parent)
    : QDialog(parent) {
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setFixedSize(340, 196);
    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, bodyBg());
    setPalette(pal);

    // Cabecalho Navy com o titulo.
    auto* header = new QWidget(this);
    header->setGeometry(0, 0, 340, 46);
    header->setAutoFillBackground(true);
    QPalette hp = header->palette();
    hp.setColor(QPalette::Window, Navy);
    header->setPalette(hp);

    auto* titleLbl = new QLabel(title, header);
    titleLbl->setGeometry(16, 0, 308, 46);
    titleLbl->setFont(uiPt(12));
    titleLbl->setStyleSheet("color:#FFFFFF;");
    titleLbl->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);

    // Rotulo do campo.
    auto* lbl = new QLabel(label, this);
    lbl->setGeometry(20, 60, 300, 18);
    lbl->setFont(uiPt(9.5));
    lbl->setStyleSheet(QStringLiteral("color:%1;").arg(textSecondary().name()));

    // Campo de entrada.
    m_edit = new QLineEdit(this);
    m_edit->setGeometry(20, 82, 300, 34);
    m_edit->setFont(uiPt(13));
    if (!placeholder.isEmpty()) m_edit->setPlaceholderText(placeholder);
    if (password) m_edit->setEchoMode(QLineEdit::Password);
    if (digitsOnly)
        m_edit->setValidator(new QRegularExpressionValidator(
            QRegularExpression(QStringLiteral("[0-9*#+]*")), m_edit));
    m_edit->setStyleSheet(QStringLiteral(
        "QLineEdit{background:%1;border:1px solid %2;border-radius:8px;padding:4px 10px;color:%3;}")
        .arg(panelGray().name(), border().name(), textPrimary().name()));

    // Botoes: Cancelar (cinza) + OK/confirmar (Cyan).
    auto* cancel = new ActionButton(this);
    cancel->setGeometry(20, 142, 140, 36);
    cancel->fill = panelGray();
    cancel->textColor = textPrimary();
    cancel->setText(QStringLiteral("Cancelar"));
    connect(cancel, &ActionButton::clicked, this, &QDialog::reject);

    auto* ok = new ActionButton(this);
    ok->setGeometry(180, 142, 140, 36);
    ok->fill = Cyan;
    ok->setText(okText);
    connect(ok, &ActionButton::clicked, this, &QDialog::accept);

    connect(m_edit, &QLineEdit::returnPressed, this, &QDialog::accept);
    m_edit->setFocus();
}

QString PromptDialog::value() const {
    return m_edit ? m_edit->text() : QString();
}

}  // namespace sphone
