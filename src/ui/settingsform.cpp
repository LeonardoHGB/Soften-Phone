#include "ui/settingsform.h"
#include "ui/controls.h"
#include "ui/updateform.h"
#include "core/brand.h"
#include "core/version.h"

#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWidget>
#include <QMessageBox>
#include <QKeyEvent>

using namespace brand;

namespace {

constexpr int kW = 360;   // largura util do conteudo (dialogo = 400, margem 20)

void setBg(QWidget* w, const QColor& c) {
    w->setAutoFillBackground(true);
    QPalette p = w->palette();
    p.setColor(QPalette::Window, c);
    w->setPalette(p);
}

QLabel* sectionHeader(const QString& text) {
    auto* l = new QLabel(text.toUpper());
    l->setFont(brand::semiPt(8.5));
    l->setStyleSheet(QStringLiteral("color:%1;background:transparent;").arg(brand::textSecondary().name()));
    l->setContentsMargins(2, 12, 0, 4);
    return l;
}

}  // namespace

namespace sphone {

SettingsForm::SettingsForm(SipConfig* config, QWidget* parent)
    : QDialog(parent), m_config(config) {
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setWindowTitle(QString::fromUtf8("Configurações - Soften Phone"));
    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, bodyBg());
    setPalette(pal);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->setSizeConstraint(QLayout::SetFixedSize);   // dialogo se ajusta ao conteudo

    // ---- Cabecalho Navy ----
    auto* header = new QWidget();
    header->setFixedSize(400, 58);
    setBg(header, Navy);
    auto* htitle = new QLabel(QString::fromUtf8("Configurações"), header);
    htitle->setGeometry(52, 0, 300, 58);
    htitle->setFont(uiPt(13.5));
    htitle->setStyleSheet("color:#FFFFFF;background:transparent;");
    htitle->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    root->addWidget(header);

    // ---- Conteudo ----
    auto* content = new QWidget();
    content->setFixedWidth(400);
    auto* cv = new QVBoxLayout(content);
    cv->setContentsMargins(20, 8, 20, 10);
    cv->setSpacing(0);

    auto addField = [&](const QString& label, const QString& value, bool password, QLineEdit*& out) {
        auto* lbl = new QLabel(label);
        lbl->setFont(uiPt(9.5));
        lbl->setStyleSheet(QStringLiteral("color:%1;background:transparent;").arg(textSecondary().name()));
        lbl->setContentsMargins(2, 0, 0, 2);
        cv->addWidget(lbl);

        out = new QLineEdit(value);
        out->setFont(uiPt(11.5));
        out->setFixedHeight(40);
        if (password) out->setEchoMode(QLineEdit::Password);
        out->setStyleSheet(QStringLiteral(
            "QLineEdit{background:%1;border:1px solid %2;border-radius:8px;padding:0 11px;color:%3;}")
            .arg(bodyBg().name(), border().name(), textPrimary().name()));
        cv->addWidget(out);
        cv->addSpacing(8);
    };

    cv->addWidget(sectionHeader(QStringLiteral("Conta")));
    addField(QStringLiteral("Servidor"), m_config->server, false, m_server);
    addField(QStringLiteral("Ramal"), m_config->username, false, m_user);
    addField(QStringLiteral("Senha"), m_config->password, true, m_pass);

    cv->addWidget(sectionHeader(QString::fromUtf8("Aparência")));
    {
        auto* card = new RoundedCard();
        card->fillColor = panelGray();
        card->borderColor = border();
        card->borderThickness = 1;
        card->radius = 8;
        card->setFixedSize(kW, 46);
        auto* ch = new QHBoxLayout(card);
        ch->setContentsMargins(14, 0, 14, 0);
        auto* tl = new QLabel(QStringLiteral("Tema escuro"));
        tl->setFont(uiPt(10.5));
        tl->setStyleSheet(QStringLiteral("color:%1;background:transparent;").arg(textPrimary().name()));
        m_dark = new ToggleSwitch();
        m_dark->setChecked(m_config->darkTheme);
        ch->addWidget(tl);
        ch->addStretch();
        ch->addWidget(m_dark);
        cv->addWidget(card);
        cv->addSpacing(8);
    }

    // ---- Atualizacao (verificacao real entra na fase 8) ----
    {
        cv->addSpacing(4);
        auto* upd = new ActionButton();
        upd->setText(QString::fromUtf8("Procurar atualização"));
        upd->fill = panelGray();
        upd->textColor = textPrimary();
        upd->radius = 12;
        upd->setFixedSize(kW, 42);
        connect(upd, &ActionButton::clicked, this, [this] { runUpdateCheck(this, /*silent*/ false); });
        cv->addWidget(upd);
        auto* ver = new QLabel(QString::fromUtf8("Versão atual: v") + QStringLiteral(SPHONE_VERSION));
        ver->setFont(uiPt(8.5));
        ver->setStyleSheet(QStringLiteral("color:%1;background:transparent;").arg(textTertiary().name()));
        ver->setAlignment(Qt::AlignCenter);
        ver->setContentsMargins(0, 6, 0, 0);
        cv->addWidget(ver);
    }

    root->addWidget(content);

    // ---- Rodape (linha + Cancelar + Salvar) ----
    auto* footer = new QWidget();
    footer->setFixedSize(400, 66);
    setBg(footer, bodyBg());
    auto* line = new QWidget(footer);
    line->setGeometry(0, 0, 400, 1);
    setBg(line, border());

    auto* salvar = new ActionButton(footer);
    salvar->setText(QStringLiteral("Salvar"));
    salvar->fill = Cyan;
    salvar->radius = 12;
    salvar->setGeometry(20 + kW - 130, 14, 130, 42);
    connect(salvar, &ActionButton::clicked, this, [this] { save(); });

    auto* cancelar = new ActionButton(footer);
    cancelar->setText(QStringLiteral("Cancelar"));
    cancelar->fill = panelGray();
    cancelar->textColor = textPrimary();
    cancelar->radius = 12;
    cancelar->setGeometry(20 + kW - 130 - 116, 14, 106, 42);
    connect(cancelar, &ActionButton::clicked, this, &QDialog::reject);

    root->addWidget(footer);
}

void SettingsForm::keyPressEvent(QKeyEvent* e) {
    if (e->key() == Qt::Key_Escape) { reject(); return; }
    // Enter fora de um campo de texto -> salva (campos consomem o Enter normalmente).
    if ((e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter)
        && !qobject_cast<QLineEdit*>(focusWidget())) {
        save();
        return;
    }
    QDialog::keyPressEvent(e);
}

void SettingsForm::save() {
    if (m_server->text().trimmed().isEmpty() ||
        m_user->text().trimmed().isEmpty() ||
        m_pass->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, QString::fromUtf8("Campos obrigatórios"),
            QString::fromUtf8("Preencha ao menos Servidor, Ramal e Senha."));
        return;
    }
    m_config->server   = m_server->text().trimmed();
    m_config->username = m_user->text().trimmed();
    m_config->password = m_pass->text();
    m_config->darkTheme = m_dark->isChecked();
    accept();
}

}  // namespace sphone
