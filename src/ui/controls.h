#pragma once
//
// controls.h — Controles custom desenhados com QPainter (porte de UiControls.cs).
//
// Cada controle replica radii, blends (hover/down), fontes pt/px, paddings e
// glifos MDL2 exatos do original. As cores vem de brand.h (lidas na pintura).
//
#include <QWidget>
#include <QString>
#include <QColor>

namespace sphone {

// ClickableWidget: QWidget generico que emite clicked() no mouse-up interno.
// Usado nas linhas do historico (clique em qualquer parte -> rediscar).
class ClickableWidget : public QWidget {
    Q_OBJECT
public:
    explicit ClickableWidget(QWidget* parent = nullptr);
signals:
    void clicked();
protected:
    void mouseReleaseEvent(QMouseEvent*) override;
};

// Card: retangulo arredondado com preenchimento e borda opcionais.
class RoundedCard : public QWidget {
    Q_OBJECT
public:
    explicit RoundedCard(QWidget* parent = nullptr);
    int    radius = 12;
    QColor fillColor = Qt::white;
    QColor borderColor = Qt::transparent;
    double borderThickness = 0;
protected:
    void paintEvent(QPaintEvent*) override;
};

// IconButton: botao circular com um glifo central (atender/recusar/encerrar).
class IconButton : public QWidget {
    Q_OBJECT
public:
    explicit IconButton(QWidget* parent = nullptr);
    QColor  fill = Qt::gray;
    QColor  glyphColor = Qt::white;
    QString glyph;
    double  glyphSize = 22;
    double  glyphRotation = 0;
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

// ActionButton: pilula com glifo + texto (Ligar / Encerrar / Salvar).
class ActionButton : public QWidget {
    Q_OBJECT
public:
    explicit ActionButton(QWidget* parent = nullptr);
    QString text() const { return m_text; }
    void    setText(const QString& t) { m_text = t; update(); }
    QColor  fill;             // default Brand.Cyan (definido no .cpp)
    QColor  textColor = Qt::white;
    QString glyph;
    double  glyphSize = 18;
    double  glyphRotation = 0;
    int     radius = 12;
signals:
    void clicked();
protected:
    void paintEvent(QPaintEvent*) override;
    void enterEvent(QEnterEvent*) override;
    void leaveEvent(QEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
private:
    QString m_text;
    bool m_hover = false, m_down = false;
};

// DialKey: tecla do teclado numerico (numero grande + letras pequenas).
class DialKey : public QWidget {
    Q_OBJECT
public:
    explicit DialKey(QWidget* parent = nullptr);
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

// CallControl: item da tela "Em chamada" (glifo + rotulo, com estado ativo).
class CallControl : public QWidget {
    Q_OBJECT
public:
    explicit CallControl(QWidget* parent = nullptr);
    QString glyph;
    QString label;
    bool    active() const { return m_active; }
    void    setActive(bool a) { m_active = a; update(); }
signals:
    void clicked();
protected:
    void paintEvent(QPaintEvent*) override;
    void enterEvent(QEnterEvent*) override;
    void leaveEvent(QEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
private:
    bool m_hover = false, m_active = false;
};

// ToggleSwitch: interruptor liga/desliga (46x26).
class ToggleSwitch : public QWidget {
    Q_OBJECT
public:
    explicit ToggleSwitch(QWidget* parent = nullptr);
    bool isChecked() const { return m_checked; }
    void setChecked(bool c) { if (m_checked != c) { m_checked = c; update(); emit toggled(m_checked); } }
signals:
    void toggled(bool checked);
protected:
    void paintEvent(QPaintEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
private:
    bool m_checked = false;
};

enum class StatusLevel { Ok, Warn, Error };

// StatusIndicator: circulo colorido + texto (verde/amarelo/vermelho).
class StatusIndicator : public QWidget {
    Q_OBJECT
public:
    explicit StatusIndicator(QWidget* parent = nullptr);
    StatusLevel level() const { return m_level; }
    void setLevel(StatusLevel l) { if (m_level != l) { m_level = l; update(); } }
    QString text() const { return m_text; }
    void setText(const QString& t) { m_text = t; update(); }
    QColor foreground = Qt::black;   // definido pelo tema (TextSecondary)
protected:
    void paintEvent(QPaintEvent*) override;
private:
    StatusLevel m_level = StatusLevel::Ok;
    QString m_text;
};

// LevelBar: medidor VU segmentado (22 segmentos) com icone + rotulo.
class LevelBar : public QWidget {
    Q_OBJECT
public:
    explicit LevelBar(QWidget* parent = nullptr);
    QString caption;
    QString glyph;
    void setLevel(float v) { m_level = v; update(); }   // 0..1
protected:
    void paintEvent(QPaintEvent*) override;
private:
    float m_level = 0.0f;
};

// Avatar: circulo translucido com glifo de contato.
class Avatar : public QWidget {
    Q_OBJECT
public:
    explicit Avatar(QWidget* parent = nullptr);
    double glyphSize = 42;
protected:
    void paintEvent(QPaintEvent*) override;
};

}  // namespace sphone
