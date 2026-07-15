#pragma once
//
// signalwidgets.h — Widgets custom-paint do design "Signal Architecture".
//
// Folhas reutilizadas pelos paineis do shell desktop. Leem os tokens de tema
// (brand::pal()/sig()) no momento da pintura; cores dependentes de contexto
// (campo navy vs. painel claro) sao injetadas por campos publicos. Fase 1
// (visual): SignalRings/Waveform sao ESTATICOS, mas ja expoem a API
// (setActive/pushLevel/setPaused) para a fase 2 animar sem mudar chamadas.
//
#include <QWidget>
#include <QString>
#include <QColor>
#include <array>

class QVariantAnimation;

namespace sphone {

// KeypadButton — tecla circular: digito grande + letras pequenas espacadas.
// Cores vem da paleta de tema (painel claro/escuro). Emite clicked().
class KeypadButton : public QWidget {
    Q_OBJECT
public:
    explicit KeypadButton(QWidget* parent = nullptr);
    QString keyChar;
    QString letters;
signals:
    void clicked();
protected:
    void paintEvent(QPaintEvent*) override;
    void enterEvent(QEnterEvent*) override;
    void leaveEvent(QEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
private:
    bool m_hover = false, m_down = false;
};

// CallButton — botao redondo de acao primaria: gradiente (base->claro), glifo
// branco (handset), halo externo suave. Variante vermelha + rotacao p/ Encerrar.
class CallButton : public QWidget {
    Q_OBJECT
public:
    explicit CallButton(QWidget* parent = nullptr);
    QColor  base;                 // cor inferior do gradiente (ligar/encerrar)
    QColor  top;                  // cor superior do gradiente; invalida = auto (clareia base)
    QColor  glyphColor = Qt::white;  // cor do glifo (ex.: escuro sobre o dourado)
    QString glyph;
    double  glyphSize = 26;
    double  glyphRotation = 0;    // 135 = handset "no gancho" (encerrar)
    bool    glow = true;          // halo externo
signals:
    void clicked();
protected:
    void paintEvent(QPaintEvent*) override;
    void enterEvent(QEnterEvent*) override;
    void leaveEvent(QEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
private:
    bool m_hover = false, m_down = false;
};

// RoundGlyphButton — botao de glifo generico (satelites do discador, cluster da
// chamada, itens do nav rail). Circulo ou quadrado-arredondado, opcionalmente
// checkable; ativo/checked pinta com activeFill + activeGlyph.
class RoundGlyphButton : public QWidget {
    Q_OBJECT
public:
    enum class Shape { Circle, RoundedSquare };
    explicit RoundGlyphButton(QWidget* parent = nullptr);

    QString glyph;
    double  glyphSize = 18;
    double  glyphRotation = 0;
    Shape   shape = Shape::Circle;
    int     squareRadius = 12;

    QColor  idleFill = Qt::transparent;
    QColor  idleBorder = Qt::transparent;
    QColor  idleGlyph = Qt::white;
    QColor  activeFill;            // default sig().cyan (definido no .cpp)
    QColor  activeGlyph = Qt::white;

    bool    checkable = false;
    bool    isActive() const { return m_active; }
    void    setActive(bool a) { if (m_active != a) { m_active = a; update(); } }
signals:
    void clicked();
    void toggled(bool active);
protected:
    void paintEvent(QPaintEvent*) override;
    void enterEvent(QEnterEvent*) override;
    void leaveEvent(QEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
private:
    bool m_hover = false, m_down = false, m_active = false;
};

// RingAvatar — circulo com iniciais. Modo gradiente (radial cyanLight->base,
// usado na chamada ativa) ou plano (cor solida, usado nas linhas de Recentes).
class RingAvatar : public QWidget {
    Q_OBJECT
public:
    explicit RingAvatar(QWidget* parent = nullptr);
    QString initials;
    QColor  base;                 // cor da borda/edge do gradiente, ou cor plana
    bool    gradient = true;
    double  textPt = 0;           // 0 = auto (proporcional ao tamanho)
protected:
    void paintEvent(QPaintEvent*) override;
};

// SignalRings — aneis concentricos tracejados + arcos (fundo da chamada ativa).
// Fase 1: estatico. Fase 2: setActive(true) inicia o pulso (phase animada).
class SignalRings : public QWidget {
    Q_OBJECT
    Q_PROPERTY(qreal phase READ phase WRITE setPhase)
public:
    explicit SignalRings(QWidget* parent = nullptr);
    QColor color;                 // sig().cyan
    void   setActive(bool on);    // inicia/para o pulso (anima a propriedade phase)
    qreal  phase() const { return m_phase; }
    void   setPhase(qreal p) { m_phase = p; update(); }
protected:
    void paintEvent(QPaintEvent*) override;
private:
    qreal m_phase = 0.20;
    bool  m_active = false;
    QVariantAnimation* m_anim = nullptr;   // loop 0->1, easing InOutSine
};

// Waveform — faixa de barras verticais. Fase 1: envelope estatico (pseudo). API
// pushLevel/setPaused pronta p/ a fase 2 alimentar com o nivel RX real.
class Waveform : public QWidget {
    Q_OBJECT
public:
    explicit Waveform(QWidget* parent = nullptr);
    QColor color;                 // sig().cyan
public slots:
    void pushLevel(float rx);     // fase 2: alimentado por SipBridge::audioLevel
    void setPaused(bool p) { m_paused = p; update(); }
protected:
    void paintEvent(QPaintEvent*) override;
private:
    std::array<float, 56> m_bars{};
    int  m_head = 0;
    bool m_paused = false;
};

}  // namespace sphone
