#include "ui/updateform.h"
#include "core/brand.h"

#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QTimer>

using namespace brand;

namespace sphone {

UpdateForm::UpdateForm(const UpdateInfo& info, QWidget* parent) : QDialog(parent) {
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setFixedSize(380, 168);
    setAutoFillBackground(true);
    QPalette pal = palette(); pal.setColor(QPalette::Window, bodyBg()); setPalette(pal);

    auto* header = new QWidget(this);
    header->setGeometry(0, 0, 380, 46);
    header->setAutoFillBackground(true);
    QPalette hp = header->palette(); hp.setColor(QPalette::Window, Navy); header->setPalette(hp);
    auto* htitle = new QLabel(QString::fromUtf8("Atualizando o Soften Phone"), header);
    htitle->setGeometry(16, 0, 348, 46);
    htitle->setFont(uiPt(12));
    htitle->setStyleSheet("color:#FFFFFF;background:transparent;");
    htitle->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);

    m_status = new QLabel(QString::fromUtf8("Baixando a versão %1…").arg(info.version), this);
    m_status->setGeometry(20, 62, 340, 20);
    m_status->setFont(uiPt(9.5));
    m_status->setStyleSheet(QStringLiteral("color:%1;background:transparent;").arg(textSecondary().name()));

    m_bar = new QProgressBar(this);
    m_bar->setGeometry(20, 88, 340, 18);
    m_bar->setRange(0, 100);
    m_bar->setValue(0);
    m_bar->setTextVisible(true);

    m_close = new QPushButton(QStringLiteral("Fechar"), this);
    m_close->setGeometry(280, 122, 80, 32);
    m_close->hide();
    connect(m_close, &QPushButton::clicked, this, &QDialog::reject);

    m_updater = new Updater(this);
    connect(m_updater, &Updater::downloadProgress, this, [this](int pct) { m_bar->setValue(pct); });
    connect(m_updater, &Updater::downloadFailed, this, [this](const QString& msg) {
        m_status->setText(QString::fromUtf8("Falha: %1").arg(msg));
        m_bar->setEnabled(false);
        m_close->show();
    });
    // Sucesso: o Updater roda o instalador e encerra o app (qApp->quit()).

    // Comeca a baixar assim que o dialogo aparece.
    QTimer::singleShot(0, this, [this, info] { m_updater->downloadAndRun(info); });
}

// ---------------------------------------------------------------------------

void runUpdateCheck(QWidget* parent, bool silent) {
    auto* up = new Updater(parent);

    QObject::connect(up, &Updater::updateAvailable, parent, [parent, up](const UpdateInfo& info) {
        up->deleteLater();
        const QString msg = QString::fromUtf8(
            "Nova versão disponível: %1\nVersão atual: %2\n\n%3\n\nAtualizar agora? O Soften Phone será reiniciado.")
            .arg(info.version, Updater::currentVersion(), info.notes);
        if (QMessageBox::question(parent, QStringLiteral("Soften Phone"), msg) == QMessageBox::Yes) {
            UpdateForm dlg(info, parent);
            dlg.exec();
        }
    });
    QObject::connect(up, &Updater::upToDate, parent, [parent, up, silent] {
        up->deleteLater();
        if (!silent)
            QMessageBox::information(parent, QStringLiteral("Soften Phone"),
                QString::fromUtf8("Você já está na versão mais recente (v%1).").arg(Updater::currentVersion()));
    });
    QObject::connect(up, &Updater::checkFailed, parent, [parent, up, silent](const QString&) {
        up->deleteLater();
        if (!silent)
            QMessageBox::warning(parent, QStringLiteral("Soften Phone"),
                QString::fromUtf8("Não foi possível verificar atualizações agora.\nVerifique a conexão e tente de novo."));
    });

    up->checkForUpdate();
}

}  // namespace sphone
