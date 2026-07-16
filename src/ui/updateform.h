#pragma once
//
// updateform.h — Dialogo de progresso da atualizacao + checagem interativa.
//
#include <QDialog>
#include "core/updater.h"

class QLabel;
class QProgressBar;
class QPushButton;

namespace sphone {

class UpdateForm : public QDialog {
    Q_OBJECT
public:
    UpdateForm(const UpdateInfo& info, QWidget* parent = nullptr);

private:
    QLabel*       m_status = nullptr;
    QProgressBar* m_bar = nullptr;
    QPushButton*  m_close = nullptr;
    Updater*      m_updater = nullptr;
};

// Checa atualizacao com UI: se houver versao nova, baixa e instala direto (sem
// perguntar). silent=true (boot): sem ruido se ja-atualizado/offline.
void runUpdateCheck(QWidget* parent, bool silent);

}  // namespace sphone
