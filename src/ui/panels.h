#pragma once
//
// panels.h — Componentes compostos do shell desktop "Signal Architecture":
// TitleBar, NavRail e os tres paineis (Discador / Chamada ativa / Recentes).
//
// Sao widgets de VISTA: expoem acessores/sinais e deixam a orquestracao (SIP,
// timers, tray, saida) para a MainWindow. Toda a aparencia vem de brand::pal()/
// sig() + signalwidgets. Fase 1 (visual): aneis/waveform/telemetria estaticos.
//
#include <QWidget>
#include <QString>
#include <QList>

#include "data/callaudit.h"
#include "sip/pjengine.h"   // AudioDevice

class QLineEdit;
class QLabel;
class QVBoxLayout;
class QButtonGroup;
class QPushButton;
class QComboBox;

namespace sphone {

class CallButton;
class RoundGlyphButton;
class RingAvatar;
class SignalRings;
class Waveform;
class RegPill;
class CallLogModel;
class CallLogProxy;
class ToggleSwitch;
struct SipConfig;

// ---------------------------------------------------------------------------
// TitleBar — barra navy com gradiente, marca, pilula de registro e chrome.
// Janela fixa: nao arrasta o top-level; expoe apenas o gesto de fechar.
// ---------------------------------------------------------------------------
class TitleBar : public QWidget {
    Q_OBJECT
public:
    explicit TitleBar(QWidget* parent = nullptr);
    void setRegistered(bool ok, const QString& text);
    void setLocked(bool locked);   // chamada recebida: chrome desabilitado
signals:
    void closeClicked();
protected:
    void paintEvent(QPaintEvent*) override;
private:
    RegPill*     m_pill = nullptr;
    QPushButton* m_close = nullptr;
    bool     m_locked = false;
};

// ---------------------------------------------------------------------------
// NavRail — coluna de 84px com os atalhos verticais + badge do ramal.
// ---------------------------------------------------------------------------
class NavRail : public QWidget {
    Q_OBJECT
public:
    enum class Active { Home, Recents, Settings };
    explicit NavRail(QWidget* parent = nullptr);
    void setRamal(const QString& ramal);
    void setActive(Active a);   // destaca o item ativo (home/recentes/settings)
signals:
    void goHome();          // botao keypad: discador sozinho
    void toggleRecents();   // botao historico: encaixa/esconde Recentes
    void toggleSettings();  // engrenagem: encaixa/esconde Configuracoes
    void toggleTheme();
protected:
    void paintEvent(QPaintEvent*) override;
private:
    QLabel*           m_badge = nullptr;
    RoundGlyphButton* m_keypadBtn = nullptr;
    RoundGlyphButton* m_recentsBtn = nullptr;
    RoundGlyphButton* m_settingsBtn = nullptr;
};

// ---------------------------------------------------------------------------
// DialerPanel — titulo, display, teclado 3x4, botao de chamada + satelites.
// ---------------------------------------------------------------------------
class DialerPanel : public QWidget {
    Q_OBJECT
public:
    explicit DialerPanel(QWidget* parent = nullptr);
    QString number() const;
    void    setNumber(const QString& n);
    void    focusDisplay();
    void    setCallEnabled(bool on);
signals:
    void keyTone(QChar d);     // tecla pressionada -> tom DTMF (+DTMF se em chamada)
    void callRequested();
protected:
    void paintEvent(QPaintEvent*) override;
    bool eventFilter(QObject* watched, QEvent* event) override;  // tom ao digitar
private:
    void append(const QString& s);
    QLineEdit* m_display = nullptr;
    CallButton* m_call = nullptr;
};

// ---------------------------------------------------------------------------
// CallPanel — campo navy: aneis, avatar, nome/numero, timer, waveform, cluster.
// Dirigido por um enum de vista (mapeado de SipManager::LineState na MainWindow).
// ---------------------------------------------------------------------------
class CallPanel : public QWidget {
    Q_OBJECT
public:
    enum class View { Idle, Outgoing, Incoming, Active, Held };
    explicit CallPanel(QWidget* parent = nullptr);

    void setPeer(const QString& name, const QString& number);
    void setView(View v);
    void setTimerText(const QString& t);
    void setMuteActive(bool on);
    void setHoldActive(bool on);
    void pushAudioLevel(float v);   // alimenta o waveform (nivel RX/TX real)
    void resetControls();
signals:
    void hangupRequested();
    void answerRequested();
    void rejectRequested();
    void muteClicked();
    void holdClicked();
    void transferClicked();
protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;
private:
    void relayout();
    View         m_view = View::Idle;
    QString      m_name, m_number;

    SignalRings* m_rings = nullptr;
    RingAvatar*  m_avatar = nullptr;
    QLabel*      m_nameLabel = nullptr;
    QLabel*      m_numberLabel = nullptr;
    QLabel*      m_timerPill = nullptr;
    Waveform*    m_wave = nullptr;

    QWidget*          m_cluster = nullptr;   // 2x3 de controles (Active/Held)
    QWidget*          m_incoming = nullptr;  // Atender/Recusar (Incoming)
    CallButton*       m_hangup = nullptr;
    QWidget*          m_idleHint = nullptr;  // placeholder "sem chamada"
    RoundGlyphButton* m_muteBtn = nullptr;
    RoundGlyphButton* m_holdBtn = nullptr;
};

// ---------------------------------------------------------------------------
// RecentsPanel — busca, abas de filtro, lista de chamadas, rodape telemetria.
// ---------------------------------------------------------------------------
class RecentsPanel : public QWidget {
    Q_OBJECT
public:
    explicit RecentsPanel(QWidget* parent = nullptr);
    void setEntries(const QList<CallAudit>& items);
    void setTelemetry(const QString& codec, const QString& latency, const QString& signal);
    void clearTelemetry();    // volta o rodape aos marcadores neutros
signals:
    void redial(const QString& number);
protected:
    void paintEvent(QPaintEvent*) override;
private:
    QLineEdit*    m_search = nullptr;
    QButtonGroup* m_tabs = nullptr;
    CallLogModel* m_model = nullptr;
    CallLogProxy* m_proxy = nullptr;
    QLabel*       m_codecVal = nullptr;
    QLabel*       m_latVal = nullptr;
    QLabel*       m_sigVal = nullptr;
};

// ---------------------------------------------------------------------------
// SettingsPanel — Configuracoes no estilo do tema, encaixavel como o Recentes.
// Edita o SipConfig recebido; a MainWindow salva/reinicia o registro no saved().
// ---------------------------------------------------------------------------
class SettingsPanel : public QWidget {
    Q_OBJECT
public:
    explicit SettingsPanel(SipConfig* config, QWidget* parent = nullptr);
    void loadConfig();        // recarrega os campos a partir do config atual
    // Preenche os combos de audio com os dispositivos do SO e seleciona os que
    // estao no config. Lista vazia = so a opcao "Padrao do sistema".
    void setAudioDevices(const QList<AudioDevice>& devices);
signals:
    void saved();             // campos validos gravados no config
    void closed();            // cancelar/fechar
    void checkUpdate();       // procurar atualizacao
protected:
    void paintEvent(QPaintEvent*) override;
private:
    SipConfig*    m_config = nullptr;
    QLineEdit*    m_server = nullptr;
    QLineEdit*    m_user = nullptr;
    QLineEdit*    m_pass = nullptr;
    QComboBox*    m_capture = nullptr;    // microfone
    QComboBox*    m_playback = nullptr;   // alto-falante/fone
};

}  // namespace sphone
