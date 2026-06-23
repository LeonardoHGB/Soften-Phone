; setup.iss — Instalador do SPHONE (Soften Phone) via Inno Setup 6.
; Instalacao per-user (sem admin), entrada em Programas, atalhos, autostart e
; desinstalador. Empacota a pasta ..\dist gerada por tools\publish.ps1.

#define MyAppName "Soften Phone"
#define MyExeName "SPHONE.exe"
; A versao pode vir por /DMyAppVersion=x.y.z (publish-release.ps1 a passa do version.h).
#ifndef MyAppVersion
  #define MyAppVersion "1.1.0"
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
Filename: "{app}\{#MyExeName}"; Description: "Abrir o {#MyAppName} agora"; Flags: nowait postinstall skipifsilent

[UninstallRun]
; O app roda na bandeja: encerra antes de desinstalar.
Filename: "{cmd}"; Parameters: "/c taskkill /im {#MyExeName} /f"; Flags: runhidden; RunOnceId: "killsphone"
