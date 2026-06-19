<#
  Bootstrap da maquina de build do SPHONE.
  Instala TODOS os pre-requisitos (Inno Setup, CMake, Ninja, aqt+Qt) e adiciona
  as exclusoes de antivirus (Defender) para o projeto nao ser comido durante o
  desenvolvimento/teste do binario ainda nao assinado.

  Uso:
    powershell -NoProfile -ExecutionPolicy Bypass -File tools\bootstrap-build-machine.ps1
  Pode rodar como usuario normal: o script eleva SOZINHO apenas o trecho do
  Defender (Add-MpPreference exige admin); o resto instala no seu perfil.

  Parametros (opcionais):
    -QtVersion 6.8.3   -QtArch win64_msvc2022_64   -QtDir C:\Qt
#>
[CmdletBinding()]
param(
    [string]   $QtVersion = '6.8.3',
    [string]   $QtArch    = 'win64_msvc2022_64',
    [string]   $QtDir     = 'C:\Qt',
    [string[]] $QtModules = @('qtmultimedia'),
    [switch]   $SkipQt,
    [switch]   $SkipDefender
)

$ErrorActionPreference = 'Stop'

function Section($t) { Write-Host ''; Write-Host "==== $t ====" -ForegroundColor Cyan }
function Info($t)    { Write-Host "  $t" }
function Ok($t)      { Write-Host "  OK  $t" -ForegroundColor Green }
function Warn($t)    { Write-Host "  !!  $t" -ForegroundColor Yellow }
function Has($n)     { [bool](Get-Command $n -ErrorAction SilentlyContinue) }

$projectRoot = Split-Path -Parent $PSScriptRoot   # ...\Desktop\SPHONE

# --------------------------------------------------------------------------
# 0) Exclusoes de antivirus (Windows Defender) - precisa de admin.
#    Roda num processo elevado SEPARADO para nao precisar elevar o script todo
#    (assim winget/pip/aqt instalam no SEU perfil, nao no do administrador).
# --------------------------------------------------------------------------
if (-not $SkipDefender) {
    Section 'Exclusoes de antivirus (Defender)'
    $paths = @(
        $projectRoot,                                         # arvore do SPHONE (build/obj)
        'C:\src\pjproject',                                   # fonte do PJSIP
        "$env:LOCALAPPDATA\Programs\SPHONE",                  # instalacao por-usuario (testes)
        "$env:ProgramFiles\SPHONE",                           # instalacao via Inno (machine)
        "${env:ProgramFiles(x86)}\SPHONE"
    ) | Where-Object { $_ -and $_.Trim() } | Select-Object -Unique

    # Monta um comando que sera executado elevado.
    $sb = New-Object System.Text.StringBuilder
    foreach ($p in $paths) {
        [void]$sb.AppendLine("try { Add-MpPreference -ExclusionPath '$p' -ErrorAction Stop; Write-Host '  + $p' } catch { Write-Host '  (Defender indisponivel ou ja excluido) $p' }")
    }
    [void]$sb.AppendLine("try { Add-MpPreference -ExclusionProcess 'SPHONE.exe' -ErrorAction Stop } catch {}")
    [void]$sb.AppendLine("Write-Host ''; Write-Host 'Exclusoes aplicadas. Feche esta janela.' -ForegroundColor Green; Start-Sleep -Seconds 2")
    $b64 = [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($sb.ToString()))

    try {
        $p = Start-Process powershell.exe -Verb RunAs -WindowStyle Normal -PassThru `
             -ArgumentList @('-NoProfile','-ExecutionPolicy','Bypass','-EncodedCommand', $b64)
        $p.WaitForExit()
        Ok 'Exclusoes do Defender solicitadas (confira a janela elevada).'
    } catch {
        Warn "Nao foi possivel elevar para o Defender: $($_.Exception.Message)"
        Warn 'Rode manualmente, como admin, os Add-MpPreference dos caminhos do projeto.'
    }
} else {
    Section 'Exclusoes de antivirus (Defender) - PULADO (-SkipDefender)'
}

# --------------------------------------------------------------------------
# 1) Ferramentas via winget (Inno Setup, CMake, Ninja).
# --------------------------------------------------------------------------
Section 'Ferramentas de build (winget)'
if (-not (Has winget)) {
    Warn 'winget ausente. Instale o "App Installer" pela Microsoft Store e rode de novo.'
} else {
    $pkgs = @(
        @{ id = 'JRSoftware.InnoSetup'; name = 'Inno Setup 6' },
        @{ id = 'Kitware.CMake';        name = 'CMake'        },
        @{ id = 'Ninja-build.Ninja';    name = 'Ninja'        }
    )
    foreach ($pkg in $pkgs) {
        Info "instalando $($pkg.name) ($($pkg.id))..."
        # --silent + aceitar termos; -e = match exato pelo id.
        winget install -e --id $pkg.id --accept-source-agreements --accept-package-agreements --silent 2>$null
        if ($LASTEXITCODE -eq 0)               { Ok "$($pkg.name) instalado" }
        elseif ($LASTEXITCODE -eq -1978335189) { Ok "$($pkg.name) ja estava instalado" }  # APPINSTALLER_CLI_ERROR_UPDATE_NOT_APPLICABLE
        else                                   { Warn "$($pkg.name): winget retornou $LASTEXITCODE (verifique manualmente)" }
    }
}

# --------------------------------------------------------------------------
# 2) Qt 6 via aqtinstall (sem precisar de conta Qt).
# --------------------------------------------------------------------------
if (-not $SkipQt) {
    Section "Qt $QtVersion ($QtArch) via aqtinstall"
    $qtTarget = Join-Path $QtDir "$QtVersion\msvc2022_64"
    if (Test-Path (Join-Path $qtTarget 'bin\qmake.exe')) {
        Ok "Qt ja presente em $qtTarget"
    } else {
        Info 'instalando/atualizando aqtinstall (pip)...'
        python -m pip install --upgrade --quiet aqtinstall
        if ($LASTEXITCODE -ne 0) { Warn 'pip install aqtinstall falhou.' }

        Info "baixando Qt $QtVersion -> $QtDir (pode demorar alguns minutos)..."
        $modArgs = @()
        if ($QtModules.Count -gt 0) { $modArgs = @('-m') + $QtModules }
        python -m aqt install-qt windows desktop $QtVersion $QtArch -O $QtDir @modArgs
        if ($LASTEXITCODE -eq 0 -and (Test-Path (Join-Path $qtTarget 'bin\qmake.exe'))) {
            Ok "Qt instalado em $qtTarget"
        } else {
            Warn "aqt retornou $LASTEXITCODE. Liste versoes com: python -m aqt list-qt windows desktop"
        }
    }

    # 3) Variaveis de ambiente do usuario (para o CMake achar o Qt e o windeployqt no PATH).
    Section 'Variaveis de ambiente (usuario)'
    if (Test-Path (Join-Path $qtTarget 'bin\qmake.exe')) {
        [Environment]::SetEnvironmentVariable('CMAKE_PREFIX_PATH', $qtTarget, 'User')
        [Environment]::SetEnvironmentVariable('Qt6_DIR', (Join-Path $qtTarget 'lib\cmake\Qt6'), 'User')
        $userPath = [Environment]::GetEnvironmentVariable('Path', 'User')
        $qtBin = Join-Path $qtTarget 'bin'
        if ($userPath -notlike "*$qtBin*") {
            [Environment]::SetEnvironmentVariable('Path', ($userPath.TrimEnd(';') + ';' + $qtBin), 'User')
            Ok "PATH do usuario += $qtBin"
        } else { Ok 'PATH ja contem o bin do Qt' }
        Ok "CMAKE_PREFIX_PATH = $qtTarget"
    } else {
        Warn 'Qt nao encontrado; variaveis de ambiente nao foram setadas.'
    }
} else {
    Section 'Qt - PULADO (-SkipQt)'
}

# --------------------------------------------------------------------------
# 4) Resumo / verificacao.
# --------------------------------------------------------------------------
Section 'Resumo'
function Show($n,$probe){ try { $v = & $probe 2>$null; if($v){ Ok ("{0,-12} {1}" -f $n, (($v|Select-Object -First 1))) } else { Warn "$n nao verificavel nesta sessao (reabra o terminal)" } } catch { Warn "$n ausente nesta sessao (reabra o terminal)" } }
Show 'cmake'  { cmake --version }
Show 'ninja'  { ninja --version }
$iscc = "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe"
if (Test-Path $iscc) { Ok "Inno Setup  $iscc" } else { Warn 'ISCC.exe nao encontrado (reabra o terminal apos instalar)' }
$qmake = Join-Path $QtDir "$QtVersion\msvc2022_64\bin\qmake.exe"
if (Test-Path $qmake) { Ok "Qt          $qmake" } else { Warn 'qmake nao encontrado' }

Write-Host ''
Write-Host 'Bootstrap concluido.' -ForegroundColor Green
Write-Host 'IMPORTANTE: reabra o terminal para o PATH novo (cmake/ninja/Qt) valer.' -ForegroundColor Yellow
Write-Host 'Build do SPHONE depois sera: configurar CMake com -DCMAKE_PREFIX_PATH=%CMAKE_PREFIX_PATH% no ambiente x64 do VS 2026.' -ForegroundColor Yellow
