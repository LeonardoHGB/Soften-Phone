#pragma once
//
// brand.h — Sistema visual (design tokens) do SPHONE.
//
// Porte fiel de UiControls.cs (Brand/Theme/Glyphs) do SoftenPhone. Todos os
// valores (HEX, pt/px, code points) são VERBATIM do app original — ver
// docs/SPEC-SoftenPhone-atual.md secao 2. Regra de tema: apenas as cores de
// "corpo" mudam entre claro/escuro; o cabecalho Navy e os acentos
// (Cyan/Green/Red/Amber e os textos azuis) sao invariantes.
//
#include <QColor>
#include <QFont>
#include <QString>
#include <QChar>

namespace brand {

// ---------------------------------------------------------------------------
// Acentos de marca — NAO mudam entre claro e escuro.
// ---------------------------------------------------------------------------
inline const QColor Navy  {0x01, 0x46, 0x94};   // cabecalho / topo
inline const QColor Cyan  {0x00, 0x9B, 0xDB};   // botao Ligar / Salvar / OK
inline const QColor Green {0x4A, 0xDE, 0x80};   // atender / online / toggle ON
inline const QColor Red   {0xE2, 0x4B, 0x4A};   // recusar / encerrar / erro
inline const QColor Amber {0xF5, 0xB3, 0x01};   // status "atencao"

// Textos sobre o cabecalho azul (o header continua azul nos dois temas).
inline const QColor LightBlueText {0x9F, 0xD4, 0xF0};
inline const QColor PaleBlueText  {0xCF, 0xEA, 0xFB};
inline const QColor DimBlueText   {0x6C, 0xB8, 0xE8};

// Cores especiais.
inline const QColor VuYellow      {0xF5, 0xC2, 0x42};      // segmento medio da LevelBar
inline const QColor ToggleTrackOff{0xCB, 0xD2, 0xDA};      // trilho do toggle desligado
inline const QColor AvatarFill    {255, 255, 255, 31};     // ~12% branco
inline const QColor AvatarBorder  {255, 255, 255, 64};     // ~25% branco

// Cores das embeds do Discord (RGB int, ver DiscordAudit).
inline constexpr int DiscordBlue  = 0x3498DB;   // em andamento
inline constexpr int DiscordGreen = 0x2ECC71;   // atendida
inline constexpr int DiscordRed   = 0xE74C3C;   // perdida / recusada
inline constexpr int DiscordGray  = 0x95A5A6;   // atendida em outro ramal

// ---------------------------------------------------------------------------
// Cores de "corpo" — mudam com o tema (Theme::apply).
// ---------------------------------------------------------------------------
struct Palette {
    QColor bodyBg;
    QColor panelGray;     // cartoes / campos
    QColor border;
    QColor textPrimary;
    QColor textSecondary;
    QColor textTertiary;
};

inline Palette lightPalette() {
    return {
        QColor(0xFF, 0xFF, 0xFF),
        QColor(0xF4, 0xF6, 0xF9),
        QColor(0xE3, 0xE7, 0xEC),
        QColor(0x1E, 0x24, 0x30),
        QColor(0x6B, 0x72, 0x80),
        QColor(0x9C, 0xA3, 0xAF),
    };
}

inline Palette darkPalette() {
    return {
        QColor(0x1B, 0x20, 0x29),
        QColor(0x26, 0x2D, 0x39),
        QColor(0x3A, 0x42, 0x50),
        QColor(0xEC, 0xEF, 0xF3),
        QColor(0xA6, 0xAE, 0xBB),
        QColor(0x6E, 0x76, 0x83),
    };
}

// ---------------------------------------------------------------------------
// Estado global do tema. Os controles custom leem estas cores no momento da
// construcao/pintura; ao trocar o tema em runtime a UI e RECONSTRUIDA (igual
// ao ApplyTheme do original). Chame Theme::apply() ANTES de montar a UI.
// ---------------------------------------------------------------------------
namespace detail { inline Palette g_palette = lightPalette(); inline bool g_dark = false; }

inline void applyTheme(bool dark) { detail::g_dark = dark; detail::g_palette = dark ? darkPalette() : lightPalette(); }
inline bool isDark()        { return detail::g_dark; }
inline const Palette& pal() { return detail::g_palette; }

inline QColor bodyBg()        { return detail::g_palette.bodyBg; }
inline QColor panelGray()     { return detail::g_palette.panelGray; }
inline QColor border()        { return detail::g_palette.border; }
inline QColor textPrimary()   { return detail::g_palette.textPrimary; }
inline QColor textSecondary() { return detail::g_palette.textSecondary; }
inline QColor textTertiary()  { return detail::g_palette.textTertiary; }

// ---------------------------------------------------------------------------
// Tipografia. "Segoe UI" base; "Segoe UI Semibold" e familia separada no
// Windows. Pesos: pt (point) e px (pixel) preservados conforme o original.
// ---------------------------------------------------------------------------
inline const char* FamilyUi       = "Segoe UI";
inline const char* FamilyUiSemi   = "Segoe UI Semibold";
inline const char* FamilyIcon     = "Segoe MDL2 Assets";

inline QFont uiPt(qreal pt) { QFont f(FamilyUi); f.setPointSizeF(pt); return f; }
inline QFont uiPx(int px)   { QFont f(FamilyUi); f.setPixelSize(px);  return f; }
inline QFont semiPt(qreal pt){ QFont f(FamilyUiSemi); f.setPointSizeF(pt); return f; }
inline QFont iconPx(int px) { QFont f(FamilyIcon); f.setPixelSize(px); return f; }

// ---------------------------------------------------------------------------
// Glifos da fonte Segoe MDL2 Assets (code points extraidos de UiControls.cs).
// ---------------------------------------------------------------------------
namespace glyph {
inline const QString History    = QString(QChar(0xE81C));
inline const QString Phone      = QString(QChar(0xE717));
inline const QString Settings   = QString(QChar(0xE713));
inline const QString Contact    = QString(QChar(0xE77B));
inline const QString Microphone = QString(QChar(0xE720));
inline const QString Mute       = QString(QChar(0xE74F));
inline const QString Dialpad    = QString(QChar(0xE75F));
inline const QString Pause      = QString(QChar(0xE769));
inline const QString Volume     = QString(QChar(0xE767));
inline const QString Transfer   = QString(QChar(0xE72A));
inline const QString Add        = QString(QChar(0xE710));
inline const QString Back       = QString(QChar(0xE72B));
}  // namespace glyph

// ---------------------------------------------------------------------------
// Utilitarios de pintura (Draw.Blend / Draw.Rounded do original).
// ---------------------------------------------------------------------------
inline QColor blend(const QColor& a, const QColor& b, double t) {
    return QColor(
        int(a.red()   + (b.red()   - a.red())   * t),
        int(a.green() + (b.green() - a.green()) * t),
        int(a.blue()  + (b.blue()  - a.blue())  * t));
}

// ---------------------------------------------------------------------------
// Dimensoes-chave (px). Ver spec secao 2.3 / 2.x.
// ---------------------------------------------------------------------------
namespace dim {
inline constexpr int WinW = 360,  WinH = 560;       // MainWindow client size
inline constexpr int HeaderH = 58;                  // header Dialer/History
inline constexpr int InCallTopH = 160, InCallFooterH = 120, InCallControlsH = 100;
inline constexpr int HistoryStatsH = 64;
inline constexpr int HangupH = 52;                  // botao Encerrar
inline constexpr int CallRowH = 66;                 // linha do botao Ligar (margem bottom 8)
inline constexpr int IncomingBtn = 60;              // IconButton incoming 60x60
inline constexpr int DialKeyMargin = 4;
inline constexpr int EdgeGap = 8;                   // 8px do canto inferior direito
}  // namespace dim

}  // namespace brand
