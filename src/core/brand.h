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

// Valores VERBATIM do design "Signal Architecture" (ver SOFTEN_PHONE_CPP.md §2).
// bodyBg = superficie dos paineis/cards; appBg (em SigTokens) = base da janela.
inline Palette lightPalette() {
    return {
        QColor(0xFF, 0xFF, 0xFF),   // bodyBg     white  (superficie de painel)
        QColor(0xF4, 0xF7, 0xFB),   // panelGray  paper  (display, teclas, busca)
        QColor(0xDB, 0xE4, 0xEE),   // border     greyLine
        QColor(0x0D, 0x21, 0x38),   // textPrimary   ink
        QColor(0x5B, 0x70, 0x88),   // textSecondary inkSoft
        QColor(0x8A, 0xA0, 0xB8),   // textTertiary  inkMute
    };
}

inline Palette darkPalette() {
    // Campanha "Rumo ao Hexa": base grafite premium (substitui o azul escuro).
    return {
        QColor(0x15, 0x16, 0x1a),   // bodyBg     grafite base
        QColor(0x1d, 0x1e, 0x23),   // panelGray  cartoes/campos grafite
        QColor(0x34, 0x35, 0x3c),   // border     grafite
        QColor(0xEE, 0xF4, 0xFB),   // textPrimary   branco
        QColor(0x8A, 0x8B, 0x92),   // textSecondary cinza
        QColor(0x7A, 0x7B, 0x82),   // textTertiary  cinza
    };
}

// ---------------------------------------------------------------------------
// SigTokens — paleta estendida do design "Signal Architecture" (alem da Palette
// base). Cobre o que o shell desktop usa: base da janela vs. superficie, rail,
// teclas, acentos ciano por tema, e os 3 stops dos gradientes (titlebar/campo).
// Selecionada em runtime junto com applyTheme(); custom-paint le via sig().
// ---------------------------------------------------------------------------
struct SigTokens {
    QColor appBg;        // base da janela (atras dos paineis)
    QColor rail;         // fundo do nav rail
    QColor railBorder;   // divisoria do rail
    QColor cyan;         // acao primaria (varia por tema p/ contraste)
    QColor cyanLight;    // topo de gradientes / acento luminoso
    QColor accentSub;    // sublabels tecnicas (SIP/RTP, — 01) sobre painel
    QColor green;        // recebida / presenca
    QColor red;          // encerrar / perdida
    QColor navyA, navyB, navyC;      // gradiente do TitleBar (canto sup-esq -> inf-dir)
    QColor fieldA, fieldB, fieldC;   // gradiente do campo de chamada (vertical)
    QColor onField;      // texto claro sobre o campo navy
    QColor onFieldSub;   // texto secundario sobre o campo navy
};

inline SigTokens sigLight() {
    return {
        QColor(0xEE, 0xF3, 0xF9),   // appBg
        QColor(0xF4, 0xF7, 0xFB),   // rail
        QColor(0xDB, 0xE4, 0xEE),   // railBorder
        QColor(0x00, 0x9B, 0xDB),   // cyan
        QColor(0x3F, 0xB8, 0xEA),   // cyanLight
        QColor(0x00, 0x9B, 0xDB),   // accentSub
        QColor(0x19, 0xB2, 0x7A),   // green
        QColor(0xE2, 0x45, 0x3D),   // red
        QColor(0x00, 0x47, 0x9A), QColor(0x01, 0x31, 0x68), QColor(0x00, 0x1F, 0x47),  // navy
        QColor(0x00, 0x47, 0x9A), QColor(0x01, 0x31, 0x68), QColor(0x00, 0x1F, 0x47),  // field
        QColor(0xFF, 0xFF, 0xFF),   // onField
        QColor(0x9F, 0xD4, 0xF0),   // onFieldSub
    };
}

inline SigTokens sigDark() {
    // Campanha: base grafite + acento dourado (#d4af37) no lugar do ciano.
    return {
        QColor(0x10, 0x11, 0x15),   // appBg     grafite
        QColor(0x0D, 0x0E, 0x11),   // rail      sidebar quase preto
        QColor(0x1A, 0x1B, 0x1F),   // railBorder
        QColor(0xD4, 0xAF, 0x37),   // cyan      -> dourado (acao primaria)
        QColor(0xF5, 0xD7, 0x7A),   // cyanLight -> dourado claro
        QColor(0x8A, 0x8B, 0x92),   // accentSub -> cinza (sublabels tecnicas)
        QColor(0x33, 0xE0, 0xA0),   // green     status/atender
        QColor(0xFF, 0x52, 0x49),   // red       encerrar/erro
        QColor(0x1C, 0x1D, 0x22), QColor(0x24, 0x25, 0x2B), QColor(0x2C, 0x2D, 0x34),  // navy (header grafite)
        QColor(0x1d, 0x1e, 0x23), QColor(0x19, 0x1a, 0x1f), QColor(0x15, 0x16, 0x1a),  // field (campo de chamada grafite)
        QColor(0xEE, 0xF4, 0xFB),   // onField    branco
        QColor(0x8A, 0x8B, 0x92),   // onFieldSub cinza
    };
}

// ---------------------------------------------------------------------------
// Estado global do tema. Os controles custom leem estas cores no momento da
// construcao/pintura; ao trocar o tema em runtime a UI e RECONSTRUIDA (igual
// ao ApplyTheme do original). Chame Theme::apply() ANTES de montar a UI.
// ---------------------------------------------------------------------------
namespace detail {
inline Palette g_palette = lightPalette();
inline SigTokens g_sig = sigLight();
inline bool g_dark = false;
}

inline const SigTokens& sig() { return detail::g_sig; }

inline void applyTheme(bool dark) {
    detail::g_dark = dark;
    detail::g_palette = dark ? darkPalette() : lightPalette();
    detail::g_sig = dark ? sigDark() : sigLight();
}
inline bool isDark()        { return detail::g_dark; }
inline const Palette& pal() { return detail::g_palette; }

inline QColor bodyBg()        { return detail::g_palette.bodyBg; }
inline QColor panelGray()     { return detail::g_palette.panelGray; }
inline QColor border()        { return detail::g_palette.border; }
inline QColor textPrimary()   { return detail::g_palette.textPrimary; }
inline QColor textSecondary() { return detail::g_palette.textSecondary; }
inline QColor textTertiary()  { return detail::g_palette.textTertiary; }

// ---------------------------------------------------------------------------
// Tipografia por PAPEL com fallback. O design pede DM Mono / Outfit / Work Sans
// / JetBrains Mono / Jura (OFL, embarcaveis). Enquanto os .ttf nao estao no
// .qrc, cada papel cai num substituto seguro ja instalado no Windows. Quando os
// TTF forem embarcados, pickFamily passa a achar a familia real automaticamente.
// ---------------------------------------------------------------------------
QString pickFamily(std::initializer_list<const char*> prefs, const char* fallback);

QFont fontDisplay(qreal pt);     // numeros do display / timer  (DM Mono)
QFont fontTelemetry(qreal pt);   // marcadores SIP/RTP, CODEC   (JetBrains Mono)
QFont fontPanelTitle(qreal pt);  // titulos "Discador"/"Recentes" (Outfit Bold)
QFont fontLabel(qreal pt);       // nomes de contato / rotulos  (Work Sans)
QFont fontBrandSub(qreal pt);    // sublabel "PHONE" (Jura Light, tracking alto)

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
inline const QString Search     = QString(QChar(0xE721));
inline const QString Video      = QString(QChar(0xE714));
inline const QString People     = QString(QChar(0xE716));
inline const QString Backspace  = QString(QChar(0xE750));
inline const QString Sun        = QString(QChar(0xE706));
inline const QString Moon       = QString(QChar(0xE708));
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
// --- Shell desktop "Signal Architecture" (3 paineis + rail + titlebar) ---
inline constexpr int ShellW = 1180, ShellH = 720;   // tamanho default da janela
inline constexpr int ShellMinW = 1000, ShellMinH = 640;
inline constexpr int RailW = 84;                    // largura do nav rail
inline constexpr int DialerW = 380;                 // largura da coluna do discador
inline constexpr int TitleBarH = 56;                // altura da barra de titulo
inline constexpr int WindowRadius = 16;             // cantos da janela frameless
inline constexpr int CardRadius = 14;               // display, busca, cards
inline constexpr int PanelPad = 28;                 // respiro interno dos paineis

// --- Widget de canto legado (mantido p/ compat dos controles antigos) ---
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
