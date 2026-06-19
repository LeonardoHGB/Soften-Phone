Vou sintetizar o documento de especificação completo a partir dos mapas fornecidos. Como o conteúdo já está todo nos mapas JSON, não preciso ler arquivos adicionais — vou compilar tudo preservando os valores exatos.

# Especificação Técnica — SPHONE (reescrita nativa PJSIP/C++ do SoftenPhone)

> Documento-base para reescrever do zero o softphone SIP da Soften Sistemas, hoje implementado em C#/.NET WinForms+WPF sobre um shim nativo PJSIP (`pjcore.dll`), como aplicativo **NATIVO** em C/C++ diretamente sobre PJSIP. Todos os valores (hex, px, fontes, strings, assinaturas C) são **verbatim** dos mapas de engenharia reversa. Itens não cobertos pelos mapas estão marcados **A DEFINIR**.

---

## 1. Visão geral e objetivo do produto

**Produto:** SoftenPhone (a ser reescrito como **SPHONE**) — softphone SIP corporativo da **Soften Sistemas**, voltado a atendentes ligados a um PABX Asterisk/Issabel via ramal SIP.

**Características fundamentais:**
- Janela única, fixa e compacta (360×560 px), **travada no canto inferior direito** da área de trabalho, **não arrastável nem minimizável** quando tocando ou travada.
- Toda a UI é **custom-drawn** (desenhada manualmente em OnPaint): nenhum controle nativo visível além da barra de título nativa do Windows e dos `MessageBox`/`ProgressBar` padrão.
- Quatro telas sobrepostas (Dialer, Incoming, InCall, History), só uma visível por vez.
- Fecha para a **bandeja do sistema** em vez de encerrar; **sair exige senha de supervisor** (SHA256).
- Motor SIP/RTP sobre **PJSIP (pjsua-lib)**, transporte **UDP** apenas, codecs default G.711/G.722.
- **Auditoria invisível e sempre ativa**: cada chamada é enviada a um webhook Discord fixo + histórico local.
- **Instância única**, **auto-instalação** em `%LOCALAPPDATA%`, **autostart HKCU**, **auto-update transacional** a partir de GitHub Releases.

**Objetivo da reescrita nativa:** eliminar a camada gerenciada (.NET/WinForms/WPF + P/Invoke), produzindo um binário C/C++ que fala PJSIP diretamente (sem `pjcore.dll` separado nem marshalling), preservando **fielmente** identidade visual, comportamento, fluxos SIP, persistência e infraestrutura aqui especificados.

---

## 2. Sistema visual (design system)

### 2.1 Paleta de cores (HEX exato)

**Acentos de marca (invariáveis ao tema — nunca mudam no claro/escuro):**

| Token | HEX | ARGB | Uso |
|---|---|---|---|
| Navy | `#014694` | `FromArgb(0x01,0x46,0x94)` | Cabeçalho/topo, fundo da tela Incoming, fundo do topo InCall |
| Cyan | `#009BDB` | `FromArgb(0x00,0x9B,0xDB)` | Botão "Ligar", botão "Salvar", OK/Confirmar, acento "Efetuadas" |
| Green | `#4ADE80` | `FromArgb(0x4a,0xde,0x80)` | Atender, online, "Recebidas", VU baixo, toggle ON |
| Red | `#E24B4A` | `FromArgb(0xe2,0x4b,0x4a)` | Recusar, Encerrar, "Perdidas", erro |
| Amber | `#F5B301` | `FromArgb(0xf5,0xb3,0x01)` | Status Warn |
| LightBlueText | `#9FD4F0` | `FromArgb(0x9f,0xd4,0xf0)` | Texto sobre Navy: timer, número incoming |
| PaleBlueText | `#CFEAFB` | `FromArgb(0xcf,0xea,0xfb)` | Ícones do header, ramal, captions |
| DimBlueText | `#6CB8E8` | `FromArgb(0x6c,0xb8,0xe8)` | Texto "tocando…" |
| VU amarelo | `#F5C242` | `FromArgb(0xF5,0xC2,0x42)` | Segmento médio da LevelBar |
| ToggleSwitch track OFF | `#CBD2DA` | `FromArgb(0xCB,0xD2,0xDA)` | Trilho do toggle desligado |

**Cores de corpo — tema CLARO (default):**

| Token | HEX |
|---|---|
| BodyBg | `#FFFFFF` |
| PanelGray (cards/campos) | `#F4F6F9` |
| Border | `#E3E7EC` |
| TextPrimary | `#1E2430` |
| TextSecondary | `#6B7280` |
| TextTertiary | `#9CA3AF` |

**Cores de corpo — tema ESCURO (`Theme.Apply(true)`):**

| Token | HEX |
|---|---|
| BodyBg | `#1B2029` |
| PanelGray | `#262D39` |
| Border | `#3A4250` |
| TextPrimary | `#ECEFF3` |
| TextSecondary | `#A6AEBB` |
| TextTertiary | `#6E7683` |

> **Regra de tema:** apenas as cores de corpo mudam. O cabeçalho Navy e todos os acentos (Cyan/Green/Red/Amber/textos azuis) permanecem fixos nos dois temas.

**Cores especiais:**
- **Avatar:** fill = `FromArgb(31,255,255,255)` (~12% branco); borda = `Pen(FromArgb(64,255,255,255), 2)` (~25% branco); glifo Contact branco.
- **Cores das embeds Discord (RGB int):** Azul `0x3498DB` (52,152,219) em andamento; Verde `0x2ECC71` (46,204,113) atendida; Vermelho `0xE74C3C` (231,76,60) perdida/recusada; Cinza `0x95A5A6` (149,165,166) atendida em outro ramal.

### 2.2 Tipografia

| Uso | Fonte | Tamanho | Peso |
|---|---|---|---|
| Fonte base do app | Segoe UI | 9.5pt | Regular |
| Título header (Dialer/Settings/Update) | Segoe UI | 13.5pt | Regular, branco |
| Título header (PromptDialog) | Segoe UI | 12pt | Regular, branco |
| Display do número | Segoe UI | 20F | Regular |
| Tecla DialKey — número | Segoe UI | 17px | Regular |
| Tecla DialKey — letras | Segoe UI | 8px | Regular |
| Botão (ActionButton) texto | Segoe UI Semibold | 11.5pt | Semibold, branco |
| Stats (números grandes do History) | Segoe UI Semibold | 14F | Semibold |
| Section headers Settings ("CONTA"/"APARENCIA") | Segoe UI Semibold | 8.5pt | Semibold, ToUpperInvariant |
| Campo de input Settings | Segoe UI | 11.5pt | — |
| Campo de input PromptDialog | Segoe UI | 13pt | — |
| Labels de campo / labels do prompt | Segoe UI | 9.5pt | — |
| Label do toggle | Segoe UI | 10.5pt | — |
| Timer da chamada (InCall) | Segoe UI | 17F | — |
| StatusIndicator | Segoe UI | 9F | — |
| Caption "Versao atual" | Segoe UI | 8.5pt | MiddleCenter |
| Ícones | **Segoe MDL2 Assets** | tamanhos por contexto (15F header, 18px botão, 22px CallControl, 14px LevelBar, 24px IconButton) | glifos PUA |

**Glifos (fonte Segoe MDL2 Assets, `Brand.IconFont`):** Phone, Settings, History, Contact, Microphone, Mute, Pause, Volume, Transfer, Back, Add, Dialpad. (Os code points PUA exatos estão em `Glyphs`/`UiControls.cs:78` — **A DEFINIR** os valores numéricos exatos.)

### 2.3 Dimensões de janela

| Janela | ClientSize | Borda | Outros |
|---|---|---|---|
| MainForm | 360×560 px | FixedSingle | MaximizeBox=false, StartPosition=Manual, travada a 8px do canto inferior direito da WorkingArea, `WS_EX_COMPOSITED (0x02000000)` |
| SettingsForm | largura 400 fixa; altura dinâmica = 58 + content.PreferredSize.Height + 66 | FixedDialog | CenterParent, sem Max/Min |
| PromptDialog | 340×196 fixo | FixedDialog | CenterScreen, sem Max/Min |
| UpdateForm | 380×168 fixo | FixedDialog | CenterScreen, sem Max/Min, **ControlBox=false** (sem botão fechar) |

**Alturas de painéis-chave (MainForm):** Header Dialer/History = 58; InCall topo azul = 160; InCall footer = 120; controlsGrid InCall = 100; stats History = 64; botão Encerrar = 52; row do botão Ligar = 66 (margem bottom 8); IconButton incoming = 60×60; margem DialKey = 4.

### 2.4 Tela DIALER (discador) — layout e controles

Root: Panel `BackColor=BodyBg`.

**Header** (Panel Dock=Top, Height=58, BackColor=Navy):
- Logo branco (PictureBox Zoom) em `Rectangle(16,15,28,28)`.
- Título "Soften Phone" branco, Segoe UI 13.5F Regular, `Rectangle(52,0,170,58)`, MiddleLeft.
- Engrenagem (Glyphs.Settings), MDL2 15F, cor PaleBlueText, `Rectangle(322,19,22,22)`, Cursor=Hand, Click→OpenSettings.
- Botão Recentes (Glyphs.History), MDL2 15F, PaleBlueText, `Rectangle(294,19,22,22)`, Click→ShowHistory.
- `_stateDot` Label "●", Segoe UI 8F, cor inicial Gray, `Rectangle(226,20,12,18)` (verde/laranja/cinza por estado/registro).
- `_ramalLabel` = `_config.Username`, Segoe UI 9.5F, PaleBlueText, `Rectangle(240,20,48,18)`.

**Corpo** (TableLayoutPanel Dock=Fill, 1 coluna × 5 linhas, Padding(16,14,16,10), BackColor=BodyBg). RowStyles: Absolute 56 (display), Percent 100 (teclado), Absolute 66 (ligar), Absolute 22 (status), Absolute 16 (versão).
- **Display:** `_displayCard` = Card FillColor=PanelGray, BorderColor=Border, BorderThickness=1, Radius=10, Margin(0,0,0,12). `_display` = TextBox Segoe UI 20F, ForeColor=TextPrimary, BackColor=PanelGray, BorderStyle=None, TextAlign=Center, PlaceholderText="Digite o número", ShortcutsEnabled=true. Centralizado vertical: Width=card.Width-28, Left=14, Top=(card.Height-display.Height)/2. KeyPress só aceita `0-9*#+`; KeyDown Enter → StartOutgoingCall.
- **Dialpad:** TableLayoutPanel 3 col (33.33%) × 4 linhas (25%), BackColor=BodyBg. 12 teclas em ordem: 1(""), 2(ABC), 3(DEF), 4(GHI), 5(JKL), 6(MNO), 7(PQRS), 8(TUV), 9(WXYZ), *(""), 0(+), #(""). Cada DialKey Dock=Fill, Margin(4). Click → `_tones.PlayDtmf(k)` + AppendToDisplay(k).
  - **Render DialKey:** cantos radius=10; fill normal=BodyBg / hover=PanelGray / down=Blend(PanelGray,Black,0.05); borda Pen(Border,1). Número Segoe UI 17px TextPrimary (rect Y=H*0.10 altura H*0.55 quando há letras, senão centro total). Letras Segoe UI 8px TextTertiary, espaçadas com espaço entre cada char, rect Y=H*0.58 altura H*0.32.
- **Botão Ligar:** `_ligarBtn` = ActionButton Text="Ligar", Glyph=Phone, Fill=Cyan, Radius=12, Margin(0,0,0,8). Click→StartOutgoingCall. Enabled=false em Offline/Registering; true em Idle.
- **StatusIndicator:** `_statusIndicator` Text=`_lastStatus`, Level inicial Warn, Segoe UI 9F, ForeColor=TextSecondary. Círculo 11px + gap 8px + texto, centralizados como conjunto. Cor do ponto: Ok=Green, Warn=Amber, Error=Red. Texto inicial "Iniciando...".
- **Versão:** Label `v{Updater.CurrentVersion}`, Segoe UI 8F, TextTertiary, MiddleCenter, Dock=Fill.

### 2.5 Tela INCOMING (chamada recebida) — layout

Root: Panel `BackColor=Navy` (tela inteira azul).
- Caption "CHAMADA RECEBIDA", Segoe UI 9.5F, LightBlueText, `Rect(0,40,360,18)`, Center.
- Avatar GlyphSize=42 em `Rect(136,84,88,88)` (centro: (360-88)/2=136).
- `_incName`, Segoe UI 15F, branco, `Rect(0,188,360,26)`.
- `_incNumber`, Segoe UI 11F, LightBlueText, `Rect(0,216,360,20)`.
- "tocando…", Segoe UI 9.5F, DimBlueText, `Rect(0,238,360,18)`.
- **Botões circulares** (IconButton 60×60) em y=430:
  - **Recusar** em x=100: Glyph=Phone rotacionado 135°, GlyphSize=24, Fill=Red, Click→`_sip.Reject()`.
  - **Atender** em x=200: Glyph=Phone, GlyphSize=24, Fill=Green, Click→`await _sip.AnswerAsync()`.
  - Captions "Recusar"/"Atender", Segoe UI 9F, PaleBlueText, abaixo em y=498 (430+60+8).

> Preenchimento de nome/número: ramal **interno** mostra nome+número; **externo** mostra só número.

### 2.6 Tela INCALL (em chamada) — layout

Root: Panel `BackColor=BodyBg`.

**Topo** (Panel Dock=Top, Height=160, BackColor=Navy):
- Avatar GlyphSize=30 em `Rect(148,22,64,64)` (centro: (360-64)/2=148).
- `_callName`, Segoe UI 13.5F, branco, `Rect(0,92,360,24)`, Center (mostra `_peerName` ou `_peerNumber`).
- `_callTimerLabel`, Segoe UI 17F, LightBlueText, `Rect(0,118,360,28)`, Center. Inicial "00:00"; vira "Chamando…" em estado Calling.

**Controles** (`_controlsGrid` TableLayoutPanel Dock=Top, Height=100, 3 col 33.33% × 1 linha). Cada CallControl via MakeControl(glyph,label,onClick), Dock=Fill, Margin(4):
- **Mudo:** Glyph=Microphone; toggle `_sip.SetMuteAsync(!IsMuted)`; Active=mute; glifo vira Mute quando ativo.
- **Espera:** Glyph=Pause; `_sip.SetHold(!IsOnHold)`; Active=hold.
- **Transferir:** Glyph=Transfer; abre `PromptDialog("Transferir chamada","Ramal de destino","Ex.: 1010", okText:"Transferir")` e chama `_sip.TransferAsync(value)`.
- **Render CallControl:** fundo arredondado radius=10 quando Active (Blend(Navy,White,0.86)) ou hover (PanelGray); ícone MDL2 22px cor Navy (rect Y=H*0.12 altura H*0.50); label Segoe UI 10px cor TextSecondary (rect Y=H*0.62 altura H*0.32).

**Footer** (Panel Dock=Bottom, Height=120), empilhado de baixo p/ cima:
- **Encerrar:** ActionButton Text="Encerrar", Glyph=Phone rotacionado 135°, Fill=Red, Radius=12, Dock=Bottom, Height=52, Click→`_sip.Hangup()`.
- spacer 12px.
- `_micBar`: LevelBar Caption="Microfone", Glyph=Microphone, Height=26.
- `_speakerBar`: LevelBar Caption="Alto-falante", Glyph=Volume, Height=26.
- **Render LevelBar:** ícone MDL2 14px cor Navy (largura 22), caption Segoe UI 9F cor TextSecondary (largura 76), VU segmentado de **22 segmentos**, gap 2px, altura 10px. Cor por fração i/22: `<0.6`=Green, `<0.85`=`#F5C242`, senão Red; segmentos apagados=Border.

### 2.7 Tela HISTORY (Recentes) — layout

- **Header** (Height=58, azul): botão Back (Glyphs.Back, MDL2 15F, PaleBlueText, `Rect(12,18,24,22)`, Click→ShowView dialer); título "Recentes", Segoe UI 13.5F, branco, `Rect(46,0,200,58)`.
- **Stats** (TableLayoutPanel Dock=Top, Height=64, 4 col 25%, Padding(6,8,6,6)). StatCell por coluna: número grande Segoe UI Semibold 14F colorido + legenda Segoe UI 8F TextSecondary. Células: **Recebidas**(Green), **Efetuadas**(Cyan), **Perdidas**(Red), **Em chamada**(Navy).
- **Lista** (`_historyList` FlowLayoutPanel Dock=Fill, TopDown, WrapContents=false, AutoScroll, Padding(0,2,0,8)). Vazio: "Nenhuma chamada registrada ainda." Segoe UI 9.5F TextTertiary.

**Linha de histórico** (Panel 322×54px, Cursor=Hand): linha divisória Pen(Border) na base (x 12..W-12). Ícone Glyphs.Phone MDL2 15F cor accent em `Rect(8,0,32,54)`. **accent:** AnsweredElsewhere=TextTertiary; senão Answered ? (inbound?Green:Cyan) : (inbound?Red:TextTertiary). Título (nome/número/"desconhecido") Segoe UI 11F TextPrimary `Rect(46,7,180,22)`. Subtítulo Segoe UI 8.5F TextSecondary `Rect(46,28,190,18)`, formato `{numero} · {Outcome}` ou `{Recebida|Efetuada} · {Outcome}`. Horário Segoe UI 9F TextTertiary `Rect(236,7,78,22)`, MiddleRight (HH:mm / "Ontem" / dd/MM). Se atendida+duração>0: duração Segoe UI 8.5F TextTertiary `Rect(236,28,78,18)`. Click (qualquer filho) → RedialFromHistory (leva número ao discador, **não liga direto**).

### 2.8 Controles custom — regras de render

- **Card:** retângulo arredondado, DoubleBuffered, AntiAlias. Inset por BorderThickness/2+0.5px antes do traço. Radius default 12 (8 para inputs). 4 arcos via Draw.Rounded.
- **ActionButton:** pílula AntiAlias+ClearTypeGridFit. Fill: disabled=Blend(Fill,White,0.45); pressed=Blend(Fill,Black,0.12); hover=Blend(Fill,White,0.06); else Fill. Cursor=Hand, não selecionável. Glifo (MDL2, +2 width) + texto, gap 9px, centralizados.
- **ToggleSwitch:** 46×26. Trilho radius 12 altura 24: ON=Green, OFF=`#CBD2DA`. Knob elipse branca diâmetro 18; OFF x=4, ON x=24 (Width-18-4); y centrado. Click alterna Checked → CheckedChanged.
- **StatusIndicator:** círculo 11px + gap 8px + texto.
- **IconButton:** botão circular; micro-press anim 1.5→3px no down.
- **Hover/down genérico:** hover clareia, down escurece, via Blend; repintura em MouseEnter/Leave/Down/Up.

### 2.9 Estados visuais de chamada (`LineState` → `ApplyState`)

| Estado | Efeito visual |
|---|---|
| **Offline** | `_stateDot`=Gray; Ligar disabled; StatusLevel=Error; ShowView dialer |
| **Registering** | `_stateDot`=Orange; Ligar disabled; StatusLevel=Warn; ShowView dialer |
| **Idle** | `_stateDot`=Green; Ligar enabled; ResetInCallControls; StatusLevel=Ok; ShowView dialer |
| **Ringing** | Preenche `_incName`/`_incNumber`; ShowView incoming; `_ringing=true`; BringToForeground; TopMost=true; StartRing (mp3+flash) |
| **Calling** | `_callName`=peer; `_callTimerLabel`="Chamando…"; ResetInCallControls; ShowView inCall; StartRingback |
| **InCall** | `_callName`=peer; ShowView inCall; StartCallTimer (500ms mm:ss); StartMeter (60ms VU peak-hold) |
| Saída de qualquer estado | !Ringing→StopRing; !InCall→StopCallTimer+StopMeter; !Calling→StopRingback; sair de Ringing solta TopMost |
| RegistrationChanged(ok) | `_stateDot.ForeColor` = ok?Green:Gray |

**Timer da chamada:** Interval=500ms, formato `{(int)t.TotalMinutes:00}:{t.Seconds:00}`. **Meter:** Interval=60ms; peak-hold `shown = nivel>shown ? nivel : shown*0.82` (ataque rápido, queda suave).

---

## 3. Catálogo de features

| Feature | Comportamento | Arquivo de origem |
|---|---|---|
| MainForm | Janela principal 360×560, FixedSingle, Font Segoe UI 9.5F, `WS_EX_COMPOSITED`, Icon do logo.png. Carrega config, tema, BuildUi, BuildTray, StartInstanceListener | MainForm.cs:10-13,62-72,98-106,172-199 |
| Barra de título travada | WndProc bloqueia SC_MINIMIZE (0xF020) enquanto ringing/locked; SC_MOVE (0xF010) quando locked; WM_NCLBUTTONDOWN+HTCAPTION(2) quando locked; WM_WINDOWPOSCHANGING força posição travada | MainForm.cs:108-166 |
| Trava de posição | `_lockedLocation = Point(WorkingArea.Right-Width-8, WorkingArea.Bottom-Height-8)`. Timer `_posGuard` 1000ms reafirma posição (desfaz minimização, reposiciona). Respeita bandeja | MainForm.cs:1505-1553 |
| Campo do número | KeyPress filtra `0-9*#+` (toca DTMF); Enter → StartOutgoingCall | MainForm.cs:306-330,1174-1209 |
| Dialpad | 12 teclas, Click → PlayDtmf + AppendToDisplay; em InCall envia DTMF | MainForm.cs:379-412; UiControls.cs:259-316 |
| Botão Ligar | StartOutgoingCall: valida sip!=null, número não-vazio, State==Idle; `_sip.CallAsync(number)` | MainForm.cs:338-349,348,1202-1219 |
| Incoming View | Atender/Recusar; preenche nome/número | MainForm.cs:416-518 |
| InCall View | Controles Mudo/Espera/Transferir + medidores + Encerrar | MainForm.cs:522-664 |
| Timer chamada | mm:ss a cada 500ms | MainForm.cs:1319-1338 |
| Meter de áudio | Lê MicLevel/SpeakerLevel, peak-hold, 60ms | MainForm.cs:1120-1147 |
| History View | Stats do dia + lista; RedialFromHistory | MainForm.cs:668-931 |
| Bandeja + menu | "Abrir"→RestoreFromTray, "Sair"→TryExit. NotifyIcon Text="Soften Phone — clique para abrir". Fechar esconde na bandeja + balão 2500ms na 1ª vez | MainForm.cs:1358-1458 |
| Sair com senha supervisor | PromptDialog password; SHA256==hash fixo → `_reallyExit`+Close | MainForm.cs:1358-1406 |
| Logo + tint branco | Extrai recurso `logo.png`; ColorMatrix força RGB→branco preservando alpha (silhueta branca p/ header azul) | MainForm.cs:953-985; UiControls.cs:463-509 |
| Ringtone + flash | StartRing extrai chamando.mp3 p/ %TEMP%, toca via WPF MediaPlayer em loop (`_ringTimer` 400ms); FlashWindowEx (FLASHW_ALL\|FLASHW_TIMERNOFG). Fallback SystemSounds.Asterisk | MainForm.cs:1555-1674 |
| OpenSettings | SettingsForm; se OK salva, atualiza ramal, re-tematiza, reinicia SIP | MainForm.cs:246,1251-1267 |
| SettingsForm | Servidor/Ramal/Senha + Tema escuro + Procurar atualização; validação; altura dinâmica | SettingsForm.cs:6-363 |
| PromptDialog | Input custom reutilizável (transfer/senha); filtro de chars de telefone | PromptDialog.cs:6-142 |
| UpdateForm | Tela de progresso de update; marquee→contínuo; SHA256; sem ControlBox | UpdateForm.cs:9-122 |
| Tones (síntese WAV) | DTMF/ringback/alarme sintetizados em memória, tocados via SoundPlayer | Audio.cs:13-165 |
| CallHistory | history.json, últimos 200, mais recente primeiro, best-effort | CallHistory.cs:21-100 |
| DiscordAudit | Webhook fixo; POST no início, PATCH no fim | DiscordAudit.cs:16-137 |
| Single instance | Mutex + EventWaitHandle | Program.cs:8-64 |
| Installer | Auto-install %LOCALAPPDATA%, sidecar, autostart, atalho | Installer.cs:13-132 |
| Updater | version.json, validação, anti-loop, .cmd transacional | Updater.cs:14-365 |
| Diag | Log em diag.txt | Diag.cs:6-21 |

---

## 4. Motor SIP/PJSIP

### 4.1 Capacidades atuais

- **Transporte:** UDP **apenas** (`PJSIP_TRANSPORT_UDP`), porta efêmera local (`port=0` em `pjcore_start`). **Sem TCP, sem TLS.**
- **Registro:** `pjsua_acc_add(reg=PJ_TRUE)`, cred realm=`*`, scheme=`digest`, data_type=`PJSIP_CRED_DATA_PLAIN_PASSWD`, `ka_interval=15` (NAT keep-alive nativo).
- **STUN/ICE:** **não configurado**. NAT mantido só por keep-alive UDP.
- **Codecs:** defaults do pjsua via `pjsua_media_config_default` (G.711 PCMU/PCMA, G.722, GSM, iLBC, Speex conforme libs); alvo Issabel/Asterisk usa G.711/G.722; OPUS/vídeo desligam se libs ausentes.
- **DTMF:** RFC2833/telephone-event (default do pjsua via `pjsua_call_dial_dtmf`), **não SIP INFO**.
- **Áudio backend:** WMME (WASAPI desligado no build).
- **180 Ringing imediato** no INVITE de entrada (antes do 200 OK).
- **Detecção "atendida em outro ramal"** via header Reason (`elsewhere` ou `cause=200`).
- **Logging:** console_level=3 sempre; arquivo `pjsip.log` level=5 (PJ_O_APPEND) só com flag `%LOCALAPPDATA%\SoftenPhone\sipdebug`.

### 4.2 Fluxo completo de inicialização e registro

1. **INIT** (`pjcore_start`, port=0): `pjsua_create` → `pjsua_config_default` (registra 5 callbacks) → `pjsua_logging_config_default` (console_level=3, level=5 em arquivo só com flag sipdebug) → `pjsua_init` → `pjsua_transport_create(UDP)` porta efêmera → `pjsua_start`. Retornos: 0 ok, -1 create, -2 init, -3 transport, -4 start.
2. **REGISTER** (`pjcore_register`): id=`sip:user@domain`, reg_uri=`sip:domain`, cred(realm=`*`,digest,PLAIN_PASSWD), ka_interval=15, `pjsua_acc_add(reg=PJ_TRUE)`. Retorna g_acc≥0 ou -1. **Domain inclui `:porta` apenas se Port≠5060.**
3. `cb_reg_state`: `code/100==2` → registered=1. OnRegState: registrado → RegistrationChanged(true), estado Idle; senão Offline + mensagem com código.

### 4.3 Fluxo de chamada de SAÍDA (outbound)

1. `CallAsync(dest)`: valida `_started`, State==Idle; NormalizeDestination (dígitos,`*`,`#`,`+`; max 32); SetState(Calling); StatusMessage "Discando para {dest}..."; inicia auditoria Outbound; StartRingback.
2. `pjcore_call`: uri `sip:dest@domain`, `pjsua_call_make_call` → call_id ou -1. id<0 → falha + CleanupCall + Idle; senão `_currentCall=id`, RaiseCallStarted.
3. `cb_call_state` CALLING/EARLY → "Chamando…"; CONFIRMED(5) → InCall, marca answered, `_callAnsweredLocal=now`, "Em chamada.", para ringback, StartCallTimer, StartMeter.
4. `cb_call_media_state` ACTIVE → `pjsua_conf_connect(conf_slot,0)` (remoto→alto-falante) e `pjsua_conf_connect(0,conf_slot)` (microfone→remoto).

### 4.4 Fluxo de chamada de ENTRADA (inbound)

1. `cb_incoming_call`: `pjsua_call_get_info` → copia `remote_info` (≤255 bytes) p/ `from[256]`; **`pjsua_call_answer(call_id,180)` (180 Ringing imediato)**; chama `g_inc_cb(call_id, from)`.
2. `OnIncomingCall`: ParseCaller extrai (número,nome) de `"Nome" <sip:num@host>`; resolve sessão pendente de fila (continuation/finalizePrev); inicia auditoria Inbound; RaiseCallStarted; IncomingCall(número,nome); SetState(Ringing).
3. **Atender** (`AnswerAsync` → `pjcore_answer`): `pjsua_call_answer(200)`. Estado vira InCall via CONFIRMED.
4. **Recusar** (`Reject` → `pjcore_hangup`): `_localHangup=true`; `pjsua_call_hangup(0)`. Volta a Idle via DISCONNECTED.

### 4.5 DTMF, Hold, Transfer, Mute

- **DTMF** (`pjcore_dtmf`): `pjsua_call_dial_dtmf`. SipManager mapeia byte 10→`*`, 11→`#`, senão dígito decimal; só envia em InCall.
- **Hold** (`pjcore_hold`): `pjsua_call_set_hold`. **Unhold** (`pjcore_unhold`): `pjsua_call_reinvite(PJSUA_CALL_UNHOLD)`.
- **Transfer** (`pjcore_transfer`): blind/REFER `pjsua_call_xfer("sip:dest@domain")`. **NÃO faz BYE local**; `cb_call_transfer_status` faz hangup da perna local no primeiro `st_code>=100` e encerra a subscrição (`*p_cont=PJ_FALSE`).
- **Mute** (`pjcore_mute`): mute → `pjsua_conf_disconnect(0,conf_slot)` (desconecta mic); unmute → `pjsua_conf_connect(0,conf_slot)`. Não afeta alto-falante.
- **Níveis** (`pjcore_get_level`): `pjsua_conf_get_signal_level` (0..255); UI normaliza `v/160f` (clamp 1.0); tx=microfone, rx=alto-falante.

### 4.6 Shutdown

`Unregister` (`pjsua_acc_set_registration FALSE`) → `Shutdown` (`pjsua_destroy`). Chamado no Dispose.

### 4.7 API atual do shim (assinaturas C exportadas)

> Na reescrita nativa estas funções **deixam de ser DLL exportada**; viram chamadas pjsua diretas. Mantidas aqui como contrato de comportamento.

```c
// Callbacks (typedefs)
typedef void (*pjcore_reg_cb)(int registered, int code);
typedef void (*pjcore_call_state_cb)(int call_id, int state, int last_code, int flags);
typedef void (*pjcore_incoming_cb)(int call_id, const char *from);

// Ciclo de vida
PJCORE_API int  pjcore_start(pjcore_reg_cb reg, pjcore_call_state_cb cs,
                             pjcore_incoming_cb inc, int port);   // 0/-1/-2/-3/-4
PJCORE_API int  pjcore_register(const char *domain, const char *user, const char *passwd); // g_acc>=0 / -1
PJCORE_API void pjcore_unregister(void);
PJCORE_API void pjcore_shutdown(void);

// Chamadas
PJCORE_API int  pjcore_call(const char *domain, const char *dest);   // call_id / -1
PJCORE_API void pjcore_answer(int call_id);                          // 200 OK
PJCORE_API void pjcore_hangup(int call_id);                          // hangup(0)
PJCORE_API void pjcore_hangup_all(void);
PJCORE_API void pjcore_dtmf(int call_id, const char *digits);
PJCORE_API void pjcore_hold(int call_id);
PJCORE_API void pjcore_unhold(int call_id);                          // reinvite UNHOLD
PJCORE_API int  pjcore_transfer(int call_id, const char *domain, const char *dest); // 0/-1 (REFER)
PJCORE_API void pjcore_mute(int call_id, int mute);
PJCORE_API int  pjcore_get_level(int call_id, int *tx, int *rx);     // 0 ok / -1 (0..255)
```

**Callbacks internos (não exportados):**
- `cb_call_transfer_status(call_id, st_code, st_text, final, *p_cont)`: 1º `st_code>=100` → hangup local + `*p_cont=PJ_FALSE`.
- `cb_call_media_state(call_id)`: ao ACTIVE, conecta conf bridge (slot 0 = dispositivo padrão Windows).
- `is_completed_elsewhere(pjsip_event*)`: header Reason com `elsewhere`/`cause=200` → flag `0x1`.
- `ensure_thread()`: TLS `__declspec(thread) pj_thread_desc`; registra thread C# via `pj_thread_register("cs",...)` no início de toda função exportada (exceto `pjcore_start`).

**Constantes `pjsip_inv_state`:** InvNull=0, InvCalling=1, InvIncoming=2, InvEarly=3, InvConnecting=4, InvConfirmed=5, InvDisconnected=6. Flag: `PJCORE_FLAG_COMPLETED_ELSEWHERE=0x1`.

### 4.8 Estados de linha e auditoria de fila

- `enum LineState { Offline, Registering, Idle, Ringing, Calling, InCall }`.
- **Coalescing de rodadas de fila** (`QueueRingGrace=30s`): rodada de fila cancelada (Ringing, !elsewhere, !localHangup, Inbound, não atendida) abre **sessão pendente** em vez de "Perdida" imediata. Mesmo número voltando na janela → continuation; outra chamada antes → finaliza anterior como perdida; janela expira → emite "Perdida" uma única vez (`FinalizePendingMiss`).

### 4.9 Mensagens de status (PT-BR, literais)

`Registrando ramal {Username}...`; `Ramal registrado com sucesso.`; `Falha no registro (codigo {code}).`; `Ramal nao registrado.`; `Chamada recebida de {from}.`; `Chamando...`; `Em chamada.`; `Atendida em outro ramal.`; `Chamada recusada.`; `Chamada perdida.`; `Chamada encerrada.`; `Discando para {dest}...`; `Numero invalido.`; `Ja existe uma chamada em andamento.`; `Nao foi possivel completar a ligacao.`; `Transferindo para {dest}...`; `Transferido para {dest}.`; `Transferencia recusada ou falhou.`; `Erro ao atender: {msg}`.

**FriendlyFailure por código SIP:** 486/600 → "Ramal ocupado."; 603 → "Chamada recusada."; 404/484 → "Numero inexistente."; 480/408 → "Ramal indisponivel."; 487 → "Chamada encerrada."; default → "Nao foi possivel completar a chamada."

### 4.10 Build do shim (referência)

CMake `add_library(pjcore SHARED pjcore.c)` linkando `pjlib pjlib-util pjnath pjmedia pjmedia-codec pjmedia-audiodev pjmedia-videodev pjsip pjsip-simple pjsua-lib`. Config: Ninja Release, CRT estático `/MT` (`CMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded`), `-DPJMEDIA_WITH_AUDIODEV_WASAPI=OFF` (usa WMME). config_site.h vazio. Saída → `native/pjcore.dll` (x64). Fonte canônico em `C:\src\pjproject\pjcore\`. DLLs de sistema: ole32, ws2_32, kernel32, winmm.

---

## 5. Áudio

### 5.1 Dispositivos

- **NÃO há enumeração nem seleção de microfone/alto-falante em lugar algum.** O PJSIP usa os **dispositivos padrão do Windows** automaticamente (`pjsua_media_config_default`, sem `pjsua_set_snd_dev`). A reescrita fiel deve **replicar essa ausência**.
- Slot 0 do conf bridge = dispositivo padrão (master). Roteamento no atendimento: `pjsua_conf_connect(conf_slot,0)` (remoto→alto-falante) e `pjsua_conf_connect(0,conf_slot)` (mic→remoto).
- Únicas referências a mic/alto-falante: medidores LevelBar e o mute (desconecta mic do bridge).

### 5.2 Sons sintetizados em memória (`Tones`, WAV PCM mono 16-bit, Rate=44100 Hz)

Tocados via `System.Media.SoundPlayer`; tudo em try/catch silencioso.

**Tabela DTMF (Hz, low/high exatos):** 1=(697,1209) 2=(697,1336) 3=(697,1477) 4=(770,1209) 5=(770,1336) 6=(770,1477) 7=(852,1209) 8=(852,1336) 9=(852,1477) *=(941,1209) 0=(941,1336) #=(941,1477) +=(941,1336) (reusa tom do 0). Cada tom: toneMs=140, amplitude=0.28, silenceMs=0.

**Ringback de saída (cadência BR):** freq 425 Hz, toneMs=1000, amplitude=0.22, silenceMs=4000 (1s tom + 4s silêncio), em loop (`PlayLooping`). Guard `_ringbackOn`.

**Alarme auto-atendimento:** 988 Hz, beepMs=130, gapMs=90, count=3, amplitude=0.35 (3 bipes).

**Síntese WAV:** soma de senos das freqs ÷ qtd; fade in/out de Rate/200 (~5ms); `pcm[i]=(short)(v*amplitude*short.MaxValue*env)`. Header RIFF/WAVE exato: 'RIFF', (36+dataBytes), 'WAVE', 'fmt ', 16, 1(PCM), 1(mono), 44100, 88200(byte rate), 2(block align), 16(bits), 'data', dataBytes(=pcm.Length*2).

### 5.3 Toque de chamada de ENTRADA (`chamando.mp3`)

- **Não é WAV sintetizado**: usa `assets/chamando.mp3` (408808 bytes) via `System.Windows.Media.MediaPlayer` (WPF) — porque SoundPlayer só toca WAV.
- StartRing: extrai o recurso embutido `chamando.mp3` p/ `%TEMP%\softenphone-chamando.mp3`; Open(Uri), Volume=1.0, Play(). **Loop manual:** Timer 400ms; ao `Position >= NaturalDuration - 120ms` reseta Position=Zero e Play(); chama KeepRingingOnTop.
- **Fallback** (MP3 falha): `SystemSounds.Asterisk.Play()` imediato + Timer 1000ms tocando a cada 3 ticks (~3s).
- StopRing: para/descarta Timer e player; `FlashWindowEx(FLASHW_STOP)`.

> **Na reescrita nativa** considerar tocar o MP3 via Media Foundation / WASAPI ou converter para WAV e usar PlaySound, eliminando a dependência WPF.

### 5.4 Medidores

`MicLevel`/`SpeakerLevel` via `ReadLevel(mic)`: se `_currentCall<0` ou State≠InCall → 0; senão `GetLevel(_currentCall, out tx, out rx)`; `n = v/160f` (clamp 1f). mic=tx, speaker=rx.

---

## 6. Persistência

### 6.1 Config (`SipConfig`, `softenphone.json`)

**Caminho:** `%LOCALAPPDATA%\SoftenPhone\softenphone.json` (Legacy: ao lado do exe, migrado).

| Campo | Tipo | Default | Observação |
|---|---|---|---|
| Server | string | "" | Servidor/IP do PABX |
| Port | int | 5060 | Domain inclui `:Port` só se ≠5060 |
| Username | string | "" | Ramal |
| Password | string | "" | **Só memória**; persistido cifrado |
| DisplayName | string | "" | Persistido mas **não usado** em `pjcore_register` (A DEFINIR uso) |
| ExpirySeconds | int | 120 | Re-registro; **não propagado** ao shim (usa default pjsua) |
| KeepAliveSeconds | int | 15 | **Não propagado**; shim usa ka_interval hardcoded=15 |
| DarkTheme | bool | false | Tema |

- **`IsComplete`** = Server && Username && Password não-vazios (gateia start do SIP).
- **Senha (DPAPI):** persistida como `PasswordProtected` base64 via `ProtectedData.Protect(UTF8, null, CurrentUser)`. Nunca em texto puro. Plaintext legado migrado e apagado. Arquivo copiado de outra máquina/usuário não decifra.
- **Modelo de exemplo** (`softenphone.example.json`): `{ _comentario, Server, Port:5060, Username, Password, DisplayName:"Atendente", ExpirySeconds:120, KeepAliveSeconds:15, DarkTheme:false }`.
- **UI Settings só edita** Server/Username/Password/DarkTheme; os demais campos mantêm valor carregado/default.

### 6.2 Histórico (`CallHistory`, `history.json`)

- **Caminho:** `%LOCALAPPDATA%\SoftenPhone\history.json`. Serialização System.Text.Json **sem opções** (PascalCase, sem indentação).
- **MaxEntries=200**; mais recente no índice 0 (`Insert(0,...)`); `RemoveRange(200, Count-200)` quando excede. Cache em memória + lock `Gate` (thread-safe). Best-effort (catch silencioso, nunca quebra a chamada).
- **`DayStats`** (record): `Incoming, Outgoing, Missed, Answered, TalkSeconds`; `Total=Incoming+Outgoing`; `AvgTalkSeconds=Answered>0?TalkSeconds/Answered:0`. `Today()` filtra `StartedLocal.Date==hoje`. **Inbound com AnsweredElsewhere=true são IGNORADAS** (não contam recebida nem perdida). Inbound: incoming++; se !Answered missed++. Outbound: outgoing++. Qualquer Answered: answered++ e talk+=DurationSeconds.

### 6.3 Auditoria (`CallAudit` + `DiscordAudit`)

**`CallAudit`** (record sealed, 9 campos posicionais nesta ordem):
```
CallDirection Direction;   // enum Inbound=0, Outbound=1 (serializado int)
string PeerNumber;         // número de quem ligou/discado
string PeerName;           // display do PABX (pode vazio)
string Ramal;              // ramal deste cliente
DateTime StartedLocal;     // hora local (ISO)
int DurationSeconds;       // 0 se não atendida
bool Answered;             // conectou
bool AnsweredElsewhere;    // atendida em outro ramal/ring group
string Outcome;            // "Atendida"/"Perdida"/"Recusada"/"Chamando"/"Tocando"...
```

**`DiscordAudit`** — webhook FIXO hardcoded no fonte:
```
<WEBHOOK_REDIGIDO>
```
- HttpClient estático, Timeout=10s, User-Agent `SoftenPhone-Audit`.
- **PostStart:** `SendNewAsync(inProgress:true)` → POST em `{url}?wait=true` (lê `id` da resposta via JsonDocument). Guarda `_pendingMessageId`.
- **PostEnd:** aguarda o id pendente; `EditOrSendAsync(inProgress:false)`: se id → PATCH `{url}/messages/{id}`; senão/falha → POST novo. Mesma mensagem se completa em tempo real.
- **Fire-and-forget:** toda falha de rede engolida em catch silencioso. Uma mensagem por chamada.

**Formato da embed (`BuildPayload`):** `{ embeds:[ { title, color, fields:[...] } ] }`.
- **title:** inProgress ? (inbound?`📞 Recebendo chamada…`:`📲 Ligando…`) : (inbound?`📞 Chamada recebida`:`📲 Chamada efetuada`). (Emojis U+1F4DE, U+1F4F2; reticências U+2026.)
- **color (int RGB):** inProgress?Blue(`0x3498DB`) : AnsweredElsewhere?Gray(`0x95A5A6`) : (Answered?Green(`0x2ECC71`):Red(`0xE74C3C`)).
- **fields (todos inline=true, nesta ordem):** `Número` (PeerNumber ou "desconhecido"); `Nome` (só se PeerName não-vazio); `Ramal` (ou "-"); `Início` (`StartedLocal.ToString("dd/MM/yyyy HH:mm:ss")`); `Duração` (só se !inProgress; FormatDuration); `Resultado` (Outcome).
- **FormatDuration:** ≤0 → "—" (U+2014); Hours>0 → "HH:mm:ss"; senão "mm:ss".

**Privacidade:** saem para servidor externo PeerNumber, PeerName, Ramal, StartedLocal, DurationSeconds, Outcome, Direction. **Nunca há gravação nem envio de áudio/conteúdo.** Auditoria sempre ativa, invisível, **sem opt-out**.

---

## 7. Infraestrutura de aplicativo

### 7.1 Caminhos, pastas e chaves (resumo)

| Item | Caminho / valor |
|---|---|
| Pasta de instalação | `%LOCALAPPDATA%\Programs\SoftenPhone` |
| Executável instalado | `%LOCALAPPDATA%\Programs\SoftenPhone\SoftenPhone.exe` |
| Sidecar nativo instalado | `%LOCALAPPDATA%\Programs\SoftenPhone\pjcore.dll` |
| Atalho desktop | `<DesktopDirectory>\Soften Phone.lnk` |
| Pasta de dados/logs | `%LOCALAPPDATA%\SoftenPhone` |
| Config | `%LOCALAPPDATA%\SoftenPhone\softenphone.json` |
| Histórico | `%LOCALAPPDATA%\SoftenPhone\history.json` |
| Log diagnóstico | `%LOCALAPPDATA%\SoftenPhone\diag.txt` |
| Estado anti-loop | `%LOCALAPPDATA%\SoftenPhone\update-state.txt` (formato `X.Y.Z N`) |
| Log de update | `%LOCALAPPDATA%\SoftenPhone\update.log` |
| Script de update | `%LOCALAPPDATA%\SoftenPhone\softenphone-apply-{guid:N}.cmd` (auto-deletado) |
| Flag debug SIP | `%LOCALAPPDATA%\SoftenPhone\sipdebug` (presença ativa pjsip.log level=5) |
| Log SIP | `%LOCALAPPDATA%\SoftenPhone\pjsip.log` |
| Staging extração | `%TEMP%\softenphone-update-{guid:N}` |
| Staging troca / backup | `<installDir>.new` / `<installDir>.old` |
| Ringtone temp | `%TEMP%\softenphone-chamando.mp3` |
| **Chave registro autostart** | `HKCU\Software\Microsoft\Windows\CurrentVersion\Run`, valor `SoftenPhone` = `"<InstallExe>"` (entre aspas) |
| Mutex instância única | `SoftenPhone.SingleInstance.v1` |
| Evento mostrar janela | `SoftenPhone.Show.v1` |
| ProgID COM atalho | `WScript.Shell` |

### 7.2 Ordem exata de boot (`Program.Main`, [STAThread])

1. `ApplicationConfiguration.Initialize()`.
2. `SetUnhandledExceptionMode(CatchException)`.
3. `ThreadException += ShowFatal`.
4. `AppDomain.UnhandledException += ShowFatal`.
5. **Só single-file:** `EnsureInstalledAndRelaunch()` (return se reabriu) → `EnsureAutoStart()` → `EnsureDesktopShortcut()`. Build dev (existe `SoftenPhone.dll` ao lado): pula tudo e loga.
6. **Mutex** `SoftenPhone.SingleInstance.v1` (`initiallyOwned=true`) — **deliberadamente após instalação** (senão a 1ª execução deadlockaria). Se `!createdNew`: abre `SoftenPhone.Show.v1` e `Set()`, return.
7. `Updater.CheckSafe()` → se update válido, `UpdateForm.ShowDialog()`; se `dlg.Restarting` → return.
8. `Application.Run(new MainForm())`.
9. `GC.KeepAlive(_instanceMutex)`.

**Instância única:** evita registro duplicado do ramal SIP. A instância existente recebe o sinal e traz a janela ao foco (consumidor do evento em MainForm).

**ShowFatal:** loga em diag.txt; MessageBox título "Soften Phone", corpo literal: `"Ocorreu um erro inesperado e a operacao foi interrompida.\n\nSe persistir, contate o suporte da Soften."` + (se ex) `"\n\nDetalhe: " + ex.Message`; OK, ícone Error. catch silencioso. Não expõe stack ao usuário.

### 7.3 Auto-instalação (`Installer`)

- `EnsureInstalledAndRelaunch`: guards `IsSingleFile`/`IsInstalledLocation`; `CreateDirectory(InstallDir)`; `File.Copy(CurrentExe, InstallExe, overwrite)`; `CopyNativeSidecars` (copia `pjcore.dll`); `EnsureAutoStart`; `EnsureDesktopShortcut`; `Process.Start(InstallExe, UseShellExecute=true)`; return true. Falha → loga e segue do local atual (degradação graciosa).
- `IsSingleFile = !File.Exists(BaseDirectory\SoftenPhone.dll)`.
- `EnsureDesktopShortcut` (idempotente): via COM `WScript.Shell` → `CreateShortcut`. TargetPath=InstallExe; WorkingDirectory=InstallDir; IconLocation=`InstallExe + ",0"`; Description="Soften Phone"; Save().
- `EnsureAutoStart`/`RemoveAutoStart`: HKCU Run, valor `SoftenPhone` = `"<InstallExe>"`. Idempotente, sem admin.

### 7.4 Auto-update transacional (`Updater`)

- **Repo:** `LeonardoHGB/SPhone`. **ManifestUrl:** `https://github.com/LeonardoHGB/SPhone/releases/latest/download/version.json`. **Pacote:** `.../SoftenPhone.zip`.
- **`CheckSafe`:** timeout 6s; rejeita se Repo começa com "OWNER"; catch→null.
- **`CheckForUpdateAsync`:** propaga exceções (UI distingue já-atualizado=null de offline=exceção).
- **`CheckAsync`:** rejeita se info null / Version vazio / Url vazio / `!IsTrustedUrl` / Sha256 vazio / latest≤CurrentVersion / anti-loop.
- **`IsTrustedUrl`:** HTTPS obrigatório + host `github.com` || `*.github.com` || `*.githubusercontent.com`.
- **`CurrentVersion`:** assembly Major.Minor.Build (`new Version(Major,Minor,Max(0,Build))`).
- **Anti-loop** (`update-state.txt`, formato `X.Y.Z N`): se versão já tentada ≥2× e app continua abaixo dela → suprime update (exe mal carimbado).
- **`DownloadAsync`:** HTTPS guard; timeout 20min; buffer 80 KiB (81920); progresso int 0..100; `%TEMP%\softenphone-update-{guid}.zip`.
- **`VerifyHash`:** SHA256 hex maiúsculo, comparação case-insensitive; hash vazio → false.
- **`ApplyAndRestart`:** extrai zip em `%TEMP%\softenphone-update-{token}`; se sem `SoftenPhone.exe` → aborta intacto. Gera `.cmd` em `%LOCALAPPDATA%\SoftenPhone` (não %TEMP%, por AV corporativo), UTF-8 sem BOM. roboFlags `/E /R:10 /W:2 /NFL /NDL /NP`. Inicia `cmd.exe /c` com WorkingDirectory=dataDir (nunca installDir), oculto.
- **Lógica do .cmd:** aguarda PID encerrar; taskkill; **PASSO1** robocopy source→`.new` (errorlevel≥8 → fail_safe), valida exe+pjcore.dll; **PASSO2** limpa `.old`; **PASSO3** move producao→`.old`; **PASSO4** move `.new`→producao (falha→:restore); **PASSO5** valida exe+pjcore.dll pós-troca (falha→:restore). Sucesso → start novo exe. **:restore** repõe `.old`. **:critical** preserva tudo (KEEPCOPIES=1). **:fail_safe** reabre producao intacta. `.old` mantido como backup até próxima atualização. Auto-delete do .cmd (`del %~f0`). echo evita `( ) > < & ^`.

### 7.5 Log de diagnóstico (`Diag`)

`%LOCALAPPDATA%\SoftenPhone\diag.txt`; `File.AppendAllText` formato `yyyy-MM-dd HH:mm:ss` + **2 espaços** + msg + `\n` (LF). catch silencioso. Sem segredos.

### 7.6 Empacotamento

- **csproj:** OutputType=WinExe; TFM=`net10.0-windows10.0.17763.0`; UseWindowsForms; UseWPF=true (p/ MediaPlayer MP3); AssemblyName/RootNamespace=SoftenPhone; ApplicationManifest=app.manifest; ApplicationIcon=`assets\logo.ico`; **Version=1.2.3**; Company="Soften Sistemas"; Product="Soften Phone"; NoWarn WFO1000. EmbeddedResource: `assets\logo.png`, `assets\chamando.mp3`. Sidecar: `None Include="native\pjcore.dll"` Link=pjcore.dll, CopyToOutputDirectory=PreserveNewest, **ExcludeFromSingleFile=true** (solto). Sem PackageReference NuGet (PJSIP substituiu SIPSorcery).
- **app.manifest:** assemblyIdentity version 1.0.0.0 name "SoftenPhone.app"; dpiAwareness **PerMonitorV2**; supportedOS Win10/11 `{8e0f7a12-bfb3-4fe8-b9a5-48fd50a15a9a}`; **sem trustInfo** → asInvoker (sem requireAdministrator).
- **publish-release.ps1:** `dotnet publish -c Release -r win-x64 --self-contained true -p:PublishSingleFile=true -p:IncludeNativeLibrariesForSelfExtract=true -p:EnableCompressionInSingleFile=true -p:DebugType=none -p:DebugSymbols=false`. Guarda: versão csproj > version.json anterior; exe carimbado == manifesto (senão throw). Copia pjcore.dll; zip sem .pdb; gera version.json com SHA256.
- **release/version.json atual:** version `1.2.3`; url `.../SoftenPhone.zip`; sha256 `53F4656AED70571AA5472E8D81F27F76E0ECC7224E42CDDF7D38AFA3F1B7226A`; notes "Atualizacao 1.2.3". Zip ~73 MB (76802385 bytes).

### 7.7 Serviço

**Não aplicável.** Não há serviço Windows; o app roda em sessão de usuário (asInvoker), autostart via HKCU Run.

---

## 8. Dependências e assets

### 8.1 Dependências (build atual)

- **PJSIP / pjsua-lib:** pjlib, pjlib-util, pjnath, pjmedia, pjmedia-codec, pjmedia-audiodev, pjmedia-videodev, pjsip, pjsip-simple, pjsua-lib.
- `native/pjcore.dll` (shim C, x64, CRT estático /MT, 1895424 bytes).
- DLLs de sistema: ole32, ws2_32, kernel32, winmm (WMME).
- (Gerenciado — a remover na reescrita nativa): System.Security.Cryptography.ProtectedData (DPAPI), System.Text.Json, System.Runtime.InteropServices, System.Windows.Media (MediaPlayer), Microsoft.Win32 (Registry), System.Net.Http, System.IO.Compression.
- Externos do .cmd: robocopy, tasklist, taskkill, timeout, cmd.exe.
- .NET 10 SDK (build atual) em `C:\Program Files\dotnet`.

### 8.2 Assets

| Asset | Tamanho | Detalhe |
|---|---|---|
| `assets/logo.png` | 68416 bytes (mapa sip-engine cita 500×500 RGBA) | Marca Soften: duas formas sugerindo um 'b'/barra inclinada — traço/barra superior ciano claro + forma curva inferior azul-marinho; fundo transparente. Renderizada 28×28 (header) / 24×24 (ico) e **tingida de branco** no header azul (ColorMatrix: RGB→0, alpha passthrough, translação RGB=1). Carregada via manifesto pelo nome que termina em `logo.png`. Define o Icon via `Icon.FromHandle(GetHicon())` |
| `assets/logo.ico` | 31674 bytes | ICO multi-resolução (7 imagens: 16×16, 24×24, … 32bpp RGBA). Mesma marca. ApplicationIcon + NotifyIcon + ícone do atalho via `{exe},0` |
| `assets/chamando.mp3` | **408808 bytes** | ID3v2.4, frame TSSE=`Lavf58.45.100` (FFmpeg libavformat 58.45), MPEG-1 Audio Layer III (`0xFF 0xFB`). Duração estimada ~25s @128kbps. Embutido; extraído p/ %TEMP% em runtime; tocado em loop (toque de ENTRADA) |

---

## 9. Riscos e pontos de atenção para a reescrita nativa

1. **Eliminação do shim P/Invoke:** em C/C++ nativo, todas as funções `pjcore_*` viram chamadas diretas a pjsua. Mas preservar **exatamente** o comportamento: `ensure_thread` deixa de ser necessário se a UI rodar na thread registrada; cuidado com callbacks pjsua chegando em threads internas do PJSIP (marshalling para a thread de UI continua obrigatório).

2. **Toda a UI é custom-drawn:** não há equivalente WinForms nativo. Reescrever os controles (Card, ActionButton, DialKey, CallControl, ToggleSwitch, StatusIndicator, LevelBar, IconButton, Avatar) em GDI+/Direct2D preservando radius, blends, fontes e posições px exatas (Seção 2). **DPI PerMonitorV2** deve ser honrado para nitidez.

3. **Fontes Segoe UI / Segoe MDL2 Assets:** dependem do Windows. Os code points PUA exatos dos glifos (`Glyphs`) **não constam dos mapas** (A DEFINIR) — extrair de `UiControls.cs:78` antes de portar.

4. **Toque MP3 sem WPF:** ao remover .NET/WPF, `MediaPlayer` some. Implementar reprodução de MP3 em loop via Media Foundation/WASAPI, ou converter `chamando.mp3` para WAV e usar a infra de WAV já existente (`Tones`/PlaySound). Manter loop a ~120ms antes do fim e fallback SystemSounds.

5. **DPAPI da senha:** `ProtectedData.Protect` (CurrentUser) é a API `CryptProtectData` (crypt32). Em C nativo usar `CryptProtectData`/`CryptUnprotectData` diretamente, preservando o formato base64 e a migração de plaintext legado para manter compatibilidade com configs existentes.

6. **Webhook Discord hardcoded é um segredo exposto no binário:** decidir se mantém embutido (risco de vazamento/abuso do webhook) ou externaliza. A privacidade (dados do interlocutor saem sem opt-out) deve ser revista juridicamente; o comportamento atual é **sempre ativo e invisível**.

7. **Campos de config não propagados:** `ExpirySeconds` e `KeepAliveSeconds` existem mas **não chegam** ao shim (ka_interval hardcoded=15, reg_timeout default). `DisplayName` persistido mas não usado na URI. Decidir na reescrita se viram parâmetros reais de registro ou permanecem ignorados (documentar a decisão).

8. **Sem TCP/TLS, sem STUN/ICE:** só UDP + keep-alive. Se houver requisito de NAT atravessável robusto ou criptografia, exigirá novo transporte — não está no escopo atual.

9. **Single-instance e auto-update transacional são críticos:** o mutex **deve** vir após a instalação (senão a 1ª execução não abre). O .cmd transacional (staging `.new` + rename atômico + `.old` backup + rollback) é a base da resiliência contra AV; preservar a lógica integral, inclusive escrever o script em `%LOCALAPPDATA%` (não %TEMP%) e WorkingDirectory ≠ installDir.

10. **Quarentena de AV em binário não-assinado:** binários nativos não-assinados podem ser bloqueados; considerar assinatura de código. O auto-update valida presença de `SoftenPhone.exe`/`pjcore.dll` — adaptar nomes para o build nativo (ex.: `SPHONE.exe` e PJSIP embutido/linkado estaticamente).

11. **PJSIP linkado estaticamente:** se o motor virar parte do exe nativo (sem `pjcore.dll` sidecar), o instalador/updater não precisará mais copiar/validar o sidecar — simplifica `CopyNativeSidecars` e os PASSOS de validação do .cmd, mas exige revisar todos os caminhos que mencionam `pjcore.dll`.

12. **Coalescing de fila (30s) e detecção "atendido em outro ramal":** lógica de negócio sutil (ring group Asterisk) que evita contar perdidas falsas. Portar `is_completed_elsewhere` (header Reason `elsewhere`/`cause=200`) e a máquina de sessão pendente fielmente, ou as estatísticas do dia ficarão erradas.

13. **Itens A DEFINIR (não cobertos pelos mapas):** code points PUA dos glifos; construtor exato de `CallAudit` já está inferido (ordem confirmada); detalhes visuais de `UpdateForm` (já cobertos pelo mapa de diálogos); consumidor exato do evento `SoftenPhone.Show.v1` em MainForm; `RemoveAutoStart` definido mas sem chamador identificado (provável desinstalação).