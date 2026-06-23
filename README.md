<div align="center">

<img src="assets/logo.png" alt="SPHONE" width="120" />

# SPHONE

### Softphone SIP nativo da **Soften Sistemas**

Reescrita **100% nativa** do SoftenPhone — de `C#/.NET` (WinForms + WPF sobre o shim `pjcore.dll`)
para **C++ moderno com Qt 6 e PJSUA2**. Mesmo visual, mesmas features, binário leve e de verdade.

<br/>

[![C++](https://img.shields.io/badge/C%2B%2B-17-00599C?style=for-the-badge&logo=cplusplus&logoColor=white)](https://isocpp.org/)
[![Qt](https://img.shields.io/badge/Qt-6.8.3_LTS-41CD52?style=for-the-badge&logo=qt&logoColor=white)](https://www.qt.io/)
[![PJSIP](https://img.shields.io/badge/PJSUA2-PJSIP-FF6C37?style=for-the-badge)](https://www.pjsip.org/)
[![Windows](https://img.shields.io/badge/Windows-10%2F11-0078D6?style=for-the-badge&logo=windows&logoColor=white)](#)

![Status](https://img.shields.io/badge/Status-Em%20Desenvolvimento-yellow?style=flat-square)
![Version](https://img.shields.io/badge/versão-1.0.0-009BDB?style=flat-square)
![Build](https://img.shields.io/badge/build-CMake_%2B_Ninja-064F8C?style=flat-square)
![Installer](https://img.shields.io/badge/instalador-Inno_Setup_6-014694?style=flat-square)

</div>

---

## ✨ Visão geral

O **SPHONE** é o softphone SIP da Soften Sistemas reescrito do zero como aplicativo
**nativo de verdade** — sem runtime gerenciado, sem `pjcore.dll` sidecar, sem P/Invoke.
A UI é redesenhada pixel a pixel com `QPainter` (custom painting), reproduzindo fielmente
a aparência do app original, e o motor SIP roda em **PJSUA2 linkado estaticamente** — *"PJSIP
na própria linguagem dele"*.

Resultado: binário leve, assinável, com instalador, desinstalador, autostart e entrada em
**Programas** — cara de aplicativo profissional, e um perfil muito melhor frente a antivírus.

<div align="center">

| 📞 Chamadas | 🎨 UI nativa | 🔔 Áudio | 🛡️ Robusto |
|:---:|:---:|:---:|:---:|
| Faz e recebe com áudio real | Custom-drawn com QPainter | DTMF, ringback e toque MP3 | Instância única, tray, autostart |

</div>

---

## 🚀 Funcionalidades

- **📲 Telefonia SIP completa** — registro no PABX, chamadas de entrada e saída com áudio bidirecional, fila de 30s e detecção de *"atendida em outro ramal"*.
- **🎛️ Controles em chamada** — mudo, espera, teclado DTMF, transferência, ajuste de volume e barra de nível (VU).
- **🎨 Interface fiel e tematizável** — 4 telas (Discador, Chamada Recebida, Em Chamada, Histórico) + diálogos de Configurações, Prompt e Atualização, com temas **claro/escuro**.
- **🔊 Áudio sintetizado** — tons DTMF e ringback gerados em tempo real + toque (`chamando.mp3`) via Qt Multimedia.
- **🗂️ Persistência** — configuração protegida com **DPAPI**, histórico em `history.json` e auditoria de chamadas.
- **📡 Auditoria no Discord** — embeds coloridas por evento (em andamento, atendida, perdida, atendida em outro ramal) via webhook.
- **🪟 Shell de janela dedicado** — frameless travada no canto, bloqueio de minimizar/mover, bandeja do sistema e **instância única**.
- **♻️ Auto-update in-app** — instalador silencioso publicado via **GitHub Releases**.

---

## 🧱 Arquitetura

Decisões centrais do porte:

| Área | Escolha | Porquê |
|---|---|---|
| **Linguagem / UI** | C++ + **Qt 6.8.3 LTS** (Widgets + Multimedia), custom-painting com `QPainter` | App nativo, melhor perfil de AV, reproduz o visual custom-drawn fielmente |
| **Motor SIP** | **PJSUA2** (API C++ do PJSIP), linkado **estático** | Elimina o `pjcore.dll` sidecar e o P/Invoke do projeto antigo |
| **Início com Windows** | Autostart `HKCU\…\Run` + entrada em **Programas** | Softphone precisa da sessão do usuário (sem Windows Service) |
| **Instalador** | **Inno Setup 6** | Gera desinstalador, atalhos, autostart e entrada em Programas; assinável |

> [!NOTE]
> A **fonte de verdade** do porte é a engenharia reversa documentada em
> [`docs/SPEC-SoftenPhone-atual.md`](docs/SPEC-SoftenPhone-atual.md) (fidelidade 88/100).
> Refinamentos em `docs/fidelidade-gaps.json` · mapas crus em `docs/mapas-raw.json`.

---

## 📁 Estrutura

```
SPHONE/
├─ docs/                 Spec mestre + gaps + mapas da engenharia reversa
├─ tools/                bootstrap-build-machine.ps1, build, publish (Qt/CMake/Ninja/Inno + AV)
├─ assets/               logo.png, logo.ico, chamando.mp3 (do app original)
├─ src/
│  ├─ core/              brand.h (design tokens), paths, log (Diag), single-instance, updater
│  ├─ sip/               PjEngine (wrapper PJSUA2) + SipManager (máquina de estados)
│  ├─ ui/                MainWindow (frameless) + 4 views + controles custom + diálogos
│  ├─ audio/             Tones (DTMF/ringback sintetizados) + Ringtone (MP3)
│  └─ data/              Config (+DPAPI), History, CallAudit, DiscordAudit
├─ packaging/            setup.iss (Inno Setup)
├─ CMakeLists.txt
└─ README.md
```

O **design system** vive em [`src/core/brand.h`](src/core/brand.h): cores, tipografia, glifos
(Segoe MDL2 Assets) e dimensões — todos os valores extraídos *verbatim* do app original.

---

## 🛠️ Compilando

### 1. Preparar a máquina de build (uma vez)

Instala Qt 6.8.3 (msvc2022_64) + Qt Multimedia, CMake, Ninja, Inno Setup 6 e configura as
exclusões de antivírus (eleva só o trecho do Defender):

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools\bootstrap-build-machine.ps1
```

> [!IMPORTANT]
> O PJSIP precisa ser compilado com **runtime dinâmico (`/MD`)** para casar com o Qt oficial.
> Use `tools\build-pjsip-md.ps1` — detalhes em [`docs/build-pjsip.md`](docs/build-pjsip.md).

### 2. Build

```powershell
# Shell visual, sem motor SIP:
cmake -B build -G Ninja
cmake --build build

# Build completo com telefonia (PJSUA2 linkado):
cmake -B build -G Ninja -DSPHONE_WITH_PJSIP=ON
cmake --build build
```

`windeployqt` é executado automaticamente após o build, copiando as DLLs do Qt para a pasta do executável.

### 3. Empacotar

```powershell
powershell -ExecutionPolicy Bypass -File tools\publish.ps1
```

Gera o instalador `SPHONE-Setup-<versão>.exe` em `packaging\Output\` — per-user, com
autostart, entrada em Programas e desinstalador.

---

## 📦 Publicando uma atualização

1. Suba a versão em [`src/core/version.h`](src/core/version.h) (`SPHONE_VERSION`) e a `FILEVERSION` / `PRODUCTVERSION` em `app.rc`.
2. Rode:
   ```powershell
   powershell -ExecutionPolicy Bypass -File tools\publish-release.ps1 -Notes "..."
   ```
3. Suba os dois assets gerados (`SPHONE-Setup-<v>.exe` e `sphone-version.json`) num **GitHub Release**.
   Os apps instalados se atualizam sozinhos. ✨

---

## 🗺️ Roadmap

- [x] Engenharia reversa + spec mestre do app atual
- [x] Bootstrap da máquina de build + exclusões de AV
- [x] Design system em C++ (`src/core/brand.h`) — cores/fontes/glifos/dimensões exatas
- [x] Build PJSIP/PJSUA2 (`/MD`) + CMake — **registra no PABX real**
- [x] Camada SIP: `PjEngine` (pjsua-lib) + `SipManager` (estados, fila 30s, "atendida em outro ramal")
- [x] Shell da janela: travada no canto, bloqueio de minimizar/mover, tray, instância única
- [x] Controles custom (Card, ActionButton, DialKey, CallControl, ToggleSwitch, StatusIndicator, LevelBar, IconButton, Avatar)
- [x] Views: Dialer, Incoming, InCall, History + diálogos Settings/Prompt/Update
- [x] Persistência: Config (+DPAPI), History, Audit + webhook Discord
- [x] Áudio: Tones (DTMF/ringback) + Ringtone MP3 (Qt Multimedia)
- [x] Empacotamento Inno Setup — per-user, autostart, Programas, desinstalador
- [x] Auto-update in-app via GitHub Releases

> **Status: completo e funcional.** Registra no PABX, faz/recebe chamadas com áudio,
> DTMF/ringback/toque, salva config+histórico, audita no Discord e instala via `SPHONE-Setup-1.0.0.exe`.

---

## 🧩 Stack

`C++17` · `Qt 6.8.3 LTS` (Widgets · Multimedia · Network) · `PJSUA2 / PJSIP` · `CMake + Ninja` ·
`MSVC` · `Inno Setup 6` · `Windows DPAPI`

---

<div align="center">

Feito com ☕ pela **Soften Sistemas**

</div>
