# Build do PJSIP/PJSUA2 para o SPHONE (Windows, MSVC, /MD x64)

O SPHONE linka o **PJSUA2** (API C++ do PJSIP) **estaticamente** no `SPHONE.exe`
— não há mais o `pjcore.dll` sidecar. Para isso o PJSIP precisa ser compilado com
o **runtime dinâmico do MSVC (`/MD`)**, igual ao Qt oficial. O `pjcore.dll` atual
foi buildado `/MT` (estático) e **não serve** aqui — refaça `/MD`.

> Fonte do PJSIP já presente em `C:\src\pjproject`.

## 1. config_site.h

Crie/edite `C:\src\pjproject\pjlib\include\pj\config_site.h` (paridade com o app atual):

```c
// SPHONE: alvo Asterisk/Issabel, audio WMME, sem video.
#define PJMEDIA_HAS_VIDEO            0
#define PJMEDIA_AUDIO_DEV_HAS_WASAPI 0   // usa WMME (igual ao shim atual)
#define PJMEDIA_AUDIO_DEV_HAS_WMME   1
// Opcional: reduzir codecs ao G.711/G.722 usados no PABX deixa o binario menor.
```

## 2. Compilar com /MD (x64 Release)

**Opção A — Visual Studio (recomendada no Windows):**
1. Abra `C:\src\pjproject\pjproject-vs14.sln` no VS Community 2026.
2. Configuração **Release / x64**.
3. Em cada projeto (ou via *property sheet*): C/C++ → Code Generation →
   **Runtime Library = Multi-threaded DLL (`/MD`)**.
4. Garanta que `config_site.h` está no include path (já está, via `pjlib`).
5. Build do projeto **`pjsua2_lib`** (puxa todas as dependências: `pjsua_lib`,
   `pjsip*`, `pjmedia*`, `pjnath`, `pjlib*`, e os `third_party`).
6. As `.lib` saem em `C:\src\pjproject\lib` e `C:\src\pjproject\third_party\lib`.

**Opção B — CMake/Ninja** (do *Developer PowerShell for VS*, ambiente x64):
```powershell
cd C:\src\pjproject
cmake -S . -B build-md -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL `
  -DPJSIP_HAS_VIDEO=OFF
cmake --build build-md
```
(As libs ficam sob `build-md`; ajuste `PJSIP_LIB_DIR` para esse caminho.)

## 3. Apontar o CMake do SPHONE

No configure do SPHONE, ligue a opção e informe a pasta + nomes das libs geradas:

```powershell
cmake -S . -B build -G Ninja `
  -DSPHONE_WITH_PJSIP=ON `
  -DPJSIP_ROOT="C:/src/pjproject" `
  -DPJSIP_LIB_DIR="C:/src/pjproject/lib" `
  -DPJSIP_LIBS="libpjsua2-x86_64-pc-windows-msvc;libpjsua-x86_64-pc-windows-msvc;libpjsip-ua-x86_64-pc-windows-msvc;libpjsip-simple-x86_64-pc-windows-msvc;libpjsip-x86_64-pc-windows-msvc;libpjmedia-codec-x86_64-pc-windows-msvc;libpjmedia-x86_64-pc-windows-msvc;libpjmedia-audiodev-x86_64-pc-windows-msvc;libpjnath-x86_64-pc-windows-msvc;libpjlib-util-x86_64-pc-windows-msvc;libpj-x86_64-pc-windows-msvc;libsrtp-x86_64-pc-windows-msvc;libresample-x86_64-pc-windows-msvc;libgsmcodec-x86_64-pc-windows-msvc;libspeex-x86_64-pc-windows-msvc;libilbccodec-x86_64-pc-windows-msvc;libg7221codec-x86_64-pc-windows-msvc;libwebrtc-x86_64-pc-windows-msvc"
```

> ⚠️ Os **sufixos dos nomes** das `.lib` variam conforme a versão do PJSIP e o
> toolset. Liste o que realmente foi gerado e ajuste `PJSIP_LIBS`:
> ```powershell
> Get-ChildItem C:\src\pjproject\lib -Filter *.lib | Select-Object Name
> ```
> Libs de sistema (ws2_32, ole32, winmm, etc.) já estão no `CMakeLists.txt`.

## 4. Sanidade

Com `SPHONE_WITH_PJSIP=ON`, o `PjEngine` (fase 2) chama `pjsua_create/_init/_start`,
cria o transporte UDP em porta efêmera e registra a conta. Um teste rápido de
registro contra o PABX confirma o link. Enquanto o build do PJSIP não está pronto,
deixe `SPHONE_WITH_PJSIP=OFF` para compilar e ver o **shell visual**.
