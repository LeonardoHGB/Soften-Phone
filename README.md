# SPHONE

Softphone SIP nativo da **Soften Sistemas** — reescrita 100% nativa do SoftenPhone
(antes C#/.NET WinForms+WPF sobre o shim `pjcore.dll`) em **C++ / Qt 6 / PJSUA2**.

Mesmo visual, mesmas features; binário nativo, leve, assinável e com cara de
aplicativo de verdade (instalação, desinstalador, autostart, entrada em Programas).

---

## Decisões de arquitetura

| Área | Escolha | Porquê |
|---|---|---|
| Linguagem/UI | **C++ + Qt 6.8.3 LTS** (Widgets + Multimedia), custom-painting com QPainter | App nativo; perfil de AV melhor; reproduz o visual custom-drawn fielmente |
| Motor SIP | **PJSUA2** (API C++ do PJSIP), linkado **estático** | "PJSIP na linguagem dele"; elimina o `pjcore.dll` sidecar e o P/Invoke |
| Início com Windows | Autostart **HKCU\…\Run** + entrada em **Programas** | Softphone precisa da sessão do usuário (sem Windows Service) |
| Instalador | **Inno Setup 6** | Gera desinstalador, atalhos, autostart e entrada em Programas; assinável |
| Assinatura | Adiada (paliativo de exclusão de AV) | Resolve depois; o sumiço no cliente era AV comendo o exe .NET não-assinado |

> Especificação fiel do app atual (fonte de verdade do porte): **`docs/SPEC-SoftenPhone-atual.md`**
> (fidelidade 88/100). Refinamentos: `docs/fidelidade-gaps.json`. Mapas crus: `docs/mapas-raw.json`.

---

## Estrutura

```
SPHONE/
├─ docs/                 Spec mestre + gaps + mapas da engenharia reversa
├─ tools/                bootstrap-build-machine.ps1 (Qt/CMake/Ninja/Inno + exclusões AV)
├─ assets/               logo.png, logo.ico, chamando.mp3 (do app original)
├─ src/
│  ├─ core/              brand.h (design tokens), paths, log (Diag), single-instance
│  ├─ sip/               PjEngine (wrapper PJSUA2) + SipManager (máquina de estados)
│  ├─ ui/                MainWindow (frameless travada) + 4 views + controles custom + diálogos
│  ├─ audio/             Tones (DTMF/ringback sintetizados) + Ringtone (MP3)
│  └─ data/              Config (+DPAPI), History, CallAudit, DiscordAudit
├─ packaging/            setup.iss (Inno Setup)
├─ CMakeLists.txt
└─ README.md
```

---

## Pré-requisitos da máquina de build

Rode uma vez (instala tudo + exclusões de AV; eleva só o trecho do Defender):

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools\bootstrap-build-machine.ps1
```

Instala: **Qt 6.8.3 (msvc2022_64) + Qt Multimedia** (via aqtinstall em `C:\Qt`),
**CMake**, **Ninja**, **Inno Setup 6** (via winget). VS Community 2026 (MSVC) e
`C:\src\pjproject` (fonte do PJSIP) já presentes.

> O PJSIP precisa ser compilado com runtime **dinâmico (/MD)** para casar com o
> Qt oficial (que usa /MD). O `pjcore.dll` atual usa `/MT` — não reaproveitar; ver
> `docs/build-pjsip.md` (a escrever).

---

## Estado do porte (roadmap)

- [x] Engenharia reversa + spec mestre do app atual
- [x] Bootstrap da máquina de build + exclusões de AV
- [x] Design system em C++ (`src/core/brand.h`) — cores/fontes/glifos/dimensões exatas
- [x] Build PJSIP/PJSUA2 (/MD) + CMake — **registra no PABX real** (`tools/build-pjsip-md.ps1`)
- [x] Camada SIP: `PjEngine` (pjsua-lib) + `SipManager` (estados, fila 30s, "atendida em outro ramal")
- [x] Shell da janela: travada no canto, bloqueio de minimizar/mover, tray, instância única
- [x] Controles custom (Card, ActionButton, DialKey, CallControl, ToggleSwitch, StatusIndicator, LevelBar, IconButton, Avatar)
- [x] Views: Dialer, Incoming, InCall, History + diálogos Settings/Prompt/Update
- [x] Persistência: Config (+DPAPI), History (history.json), Audit + webhook Discord
- [x] Áudio: Tones (DTMF/ringback contínuo) + Ringtone MP3 (Qt Multimedia)
- [x] Empacotamento Inno Setup (`packaging/setup.iss`, `tools/publish.ps1`) — per-user, autostart, Programas, desinstalador
- [x] Auto-update in-app (instalador silencioso via GitHub Releases; `tools/publish-release.ps1` gera `sphone-version.json`)

**Status: completo e funcional.** Registra no 8003@192.168.14.106, faz/recebe chamadas com áudio,
DTMF/ringback/toque, salva config+histórico, audita no Discord, instala via `SPHONE-Setup-1.0.0.exe`.

### Como publicar uma atualização
1. Bump `SPHONE_VERSION` em `src/core/version.h` e a `FILEVERSION`/`PRODUCTVERSION` em `app.rc`.
2. `powershell -ExecutionPolicy Bypass -File tools\publish-release.ps1 -Notes "..."`
3. Suba os dois assets gerados em `packaging\Output\` (`SPHONE-Setup-<v>.exe` e `sphone-version.json`)
   num GitHub Release em `LeonardoHGB/SPhone`. Os apps instalados se atualizam sozinhos.
