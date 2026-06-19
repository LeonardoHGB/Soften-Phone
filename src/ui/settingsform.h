#pragma once
//
// settingsform.h — Dialogo de Configuracoes (porte de SettingsForm.cs).
// Edita Servidor/Ramal/Senha + Tema escuro; valida e grava no SipConfig
// recebido (o chamador salva e reinicia o registro). 400px de largura,
// altura sob medida, frameless com cabecalho Navy.
//
#include <QDialog>
#include "data/sipconfig.h"

class QLineEdit;

namespace sphone {

class ToggleSwitch;

class SettingsForm : public QDialog {
    Q_OBJECT
public:
    SettingsForm(SipConfig* config, QWidget* parent = nullptr);

protected:
    void keyPressEvent(QKeyEvent*) override;

private:
    void save();

    SipConfig*    m_config = nullptr;
    QLineEdit*    m_server = nullptr;
    QLineEdit*    m_user = nullptr;
    QLineEdit*    m_pass = nullptr;
    ToggleSwitch* m_dark = nullptr;
};

}  // namespace sphone
