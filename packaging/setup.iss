; setup.iss — Instalador do SPHONE (Soften Phone) via Inno Setup 6.
; Instalacao per-user (sem admin), entrada em Programas, atalhos, autostart e
; desinstalador. Empacota a pasta ..\dist gerada por tools\publish.ps1.

#define MyAppName "Soften Phone"
#define MyExeName "SPHONE.exe"
; A versao pode vir por /DMyAppVersion=x.y.z (publish-release.ps1 a passa do version.h).
#ifndef MyAppVersion
  #define MyAppVersion "1.3.6"
#endif
#define MyPublisher "Soften Sistemas"
#define MyDist "..\dist"

[Setup]
AppId={{8E9C2A14-7F3B-4D1E-9A2C-5B6F0E1D2C30}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyPublisher}
DefaultDirName={localappdata}\Programs\SPHONE
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
OutputDir=Output
OutputBaseFilename=SPHONE-Setup-{#MyAppVersion}
SetupIconFile=..\assets\logo.ico
UninstallDisplayIcon={app}\{#MyExeName}
UninstallDisplayName={#MyAppName}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
CloseApplications=yes
RestartApplications=no
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible

[Languages]
Name: "br"; MessagesFile: "compiler:Languages\BrazilianPortuguese.isl"

[Tasks]
Name: "desktopicon"; Description: "Criar atalho na area de trabalho"; GroupDescription: "Atalhos:"
Name: "autostart"; Description: "Iniciar o {#MyAppName} junto com o Windows"; GroupDescription: "Inicializacao:"

[Files]
Source: "{#MyDist}\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs ignoreversion

[Icons]
Name: "{autoprograms}\{#MyAppName}"; Filename: "{app}\{#MyExeName}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyExeName}"; Tasks: desktopicon

[Registry]
; Autostart por-usuario (HKCU Run); removido na desinstalacao.
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; \
    ValueName: "SPHONE"; ValueData: """{app}\{#MyExeName}"""; Flags: uninsdeletevalue; Tasks: autostart

[Run]
; Instalacao interativa: checkbox "Abrir agora" na tela final (pulado se silencioso).
Filename: "{app}\{#MyExeName}"; Description: "Abrir o {#MyAppName} agora"; Flags: nowait postinstall skipifsilent
; Auto-update silencioso (/VERYSILENT): a entrada acima e pulada por skipifsilent,
; entao relancamos o app explicitamente quando a instalacao for silenciosa.
Filename: "{app}\{#MyExeName}"; Flags: nowait; Check: SilentInstall

[UninstallRun]
; O app roda na bandeja: encerra antes de desinstalar.
Filename: "{cmd}"; Parameters: "/c taskkill /im {#MyExeName} /f"; Flags: runhidden; RunOnceId: "killsphone"

[Code]
// Usado pelo [Run] para relancar o app apos a atualizacao SILENCIOSA (/VERYSILENT),
// ja que a entrada interativa "Abrir agora" e pulada por skipifsilent.
function SilentInstall: Boolean;
begin
  Result := WizardSilent;
end;

// --- Auto-update: espera o processo antigo (PID vindo do app via /WaitPid=NNN)
//     sair ANTES de trocar os arquivos, evitando o setup travar em um SPHONE.exe
//     ainda em uso. O app ja liberou a auto-protecao, entao se o encerramento
//     limpo demorar o CloseApplications=yes tambem consegue fechar. ---
const
  SYNCHRONIZE = $00100000;

function OpenProcess(dwDesiredAccess: DWORD; bInheritHandle: BOOL; dwProcessId: DWORD): THandle;
  external 'OpenProcess@kernel32.dll stdcall';
function WaitForSingleObject(hHandle: THandle; dwMilliseconds: DWORD): DWORD;
  external 'WaitForSingleObject@kernel32.dll stdcall';
function CloseHandle(hObject: THandle): BOOL;
  external 'CloseHandle@kernel32.dll stdcall';

procedure WaitForPidExit(pid: DWORD);
var
  h: THandle;
begin
  if pid = 0 then exit;
  h := OpenProcess(SYNCHRONIZE, False, pid);
  if h = 0 then exit;                 // ja saiu / sem handle
  WaitForSingleObject(h, 15000);      // teto de 15s; segue mesmo se estourar
  CloseHandle(h);
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
var
  pid: Longint;
begin
  pid := StrToIntDef(ExpandConstant('{param:WaitPid|0}'), 0);
  if pid > 0 then
    WaitForPidExit(DWORD(pid));
  Result := '';                       // vazio => prossegue com a instalacao
end;
