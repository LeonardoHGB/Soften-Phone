<#
  release-build.ps1 — Build unico que monta a pasta release-build\ com DOIS produtos:

    release-build\dev\      -> app PORTATIL (SPHONE.exe + DLLs do Qt + runtime VC).
                               E so dar duplo-clique no SPHONE.exe para testar
                               (registra SIP de verdade; nao precisa instalar).

    release-build\release\  -> instalador SPHONE-Setup-<versao>.exe + sphone-version.json.
                               Sao os DOIS assets que voce sobe no GitHub Release
                               (LeonardoHGB/SPhone) para o auto-update funcionar.

  Compila UMA vez (com PJSIP=ON), gera o dist via publish.ps1 e o instalador via
  publish-release.ps1 (ambos com -NoBuild, reaproveitando o mesmo build).

  Uso:
    powershell -NoProfile -ExecutionPolicy Bypass -File tools\release-build.ps1
    ... -Clean              recompila do zero (apaga build\)
    ... -DevOnly            so a build dev portatil (pula instalador/json)
    ... -Notes "correcoes"  texto das notas no sphone-version.json
    ... -QtDir 'C:\Qt\6.8.3\msvc2022_64'
    ... -Run                abre a build dev ao final

  Obs.: build dev e release usam PJSIP=ON. Para iteracao visual rapida SEM SIP,
  use direto:  tools\build.ps1 -Run
#>
[CmdletBinding()]
param(
    [string] $Notes  = 'Atualizacao do Soften Phone.',
    [string] $QtDir  = 'C:\Qt\6.8.3\msvc2022_64',
    [switch] $Clean,
    [switch] $DevOnly,
    [switch] $Run
)
$ErrorActionPreference = 'Stop'

$root  = Split-Path -Parent $PSScriptRoot           # ...\SPHONE
$tools = $PSScriptRoot
$build = Join-Path $root 'build'
$dist  = Join-Path $root 'dist'
$out   = Join-Path $root 'release-build'
$dev   = Join-Path $out  'dev'
$rel   = Join-Path $out  'release'

function Section($t) { Write-Host "`n==== $t ====" -ForegroundColor Cyan }

# --- Versao (fonte unica: src\core\version.h) ------------------------------
$vh = Get-Content (Join-Path $root 'src\core\version.h') -Raw
if ($vh -notmatch 'SPHONE_VERSION\s+"([^"]+)"') { throw 'Nao consegui ler SPHONE_VERSION de src\core\version.h' }
$version = $Matches[1]
Write-Host "SPHONE versao $version" -ForegroundColor Green

# --- 0) Encerra instancia em execucao (senao o link da LNK1104) ------------
$running = Get-Process SPHONE -ErrorAction SilentlyContinue
if ($running) {
    Write-Host 'Encerrando SPHONE.exe em execucao...' -ForegroundColor Yellow
    $running | Stop-Process -Force
    Start-Sleep -Milliseconds 600
}

# --- Pasta release-build limpa ---------------------------------------------
if (Test-Path $out) { Remove-Item -Recurse -Force $out }
New-Item -ItemType Directory -Force $dev | Out-Null
if (-not $DevOnly) { New-Item -ItemType Directory -Force $rel | Out-Null }

# --- 1) Compila (PJSIP=ON), uma vez ----------------------------------------
Section 'Compilando (SPHONE_WITH_PJSIP=ON)'
$buildPs1 = Join-Path $tools 'build.ps1'
if ($Clean) { & $buildPs1 -WithPjsip -QtDir $QtDir -Clean }
else        { & $buildPs1 -WithPjsip -QtDir $QtDir }
if ($LASTEXITCODE -ne 0) { throw "build falhou ($LASTEXITCODE)" }

# --- 2) Stage do dist (windeployqt + runtime VC), sem recompilar -----------
Section 'Gerando dist (windeployqt + runtime VC)'
& (Join-Path $tools 'publish.ps1') -NoBuild -QtDir $QtDir
if ($LASTEXITCODE -ne 0) { throw "publish falhou ($LASTEXITCODE)" }

# --- 3) build dev portatil = copia do dist ---------------------------------
Section 'Montando release-build\dev (portatil)'
Copy-Item (Join-Path $dist '*') $dev -Recurse -Force
Write-Host "dev pronto: $dev" -ForegroundColor Green

# --- 4) build de release = instalador + manifesto (reaproveita o dist) -----
if (-not $DevOnly) {
    Section 'Gerando instalador + sphone-version.json'
    & (Join-Path $tools 'publish-release.ps1') -NoBuild -Notes $Notes
    if ($LASTEXITCODE -ne 0) { throw "publish-release falhou ($LASTEXITCODE)" }

    $setup = Join-Path $root "packaging\Output\SPHONE-Setup-$version.exe"
    $json  = Join-Path $root 'packaging\Output\sphone-version.json'
    if (-not (Test-Path $setup)) { throw "Instalador nao encontrado: $setup" }
    Copy-Item $setup $rel -Force
    Copy-Item $json  $rel -Force
    Write-Host "release pronto: $rel" -ForegroundColor Green
}

# --- Resumo -----------------------------------------------------------------
Section 'Concluido'
$devExe = Join-Path $dev 'SPHONE.exe'
$devSize = '{0:N1} MB / {1} arquivos' -f (((Get-ChildItem $dev -Recurse -File | Measure-Object Length -Sum).Sum)/1MB),
    (Get-ChildItem $dev -Recurse -File).Count
Write-Host "  DEV (testar):  $devExe  [$devSize]"
if (-not $DevOnly) {
    Write-Host "  RELEASE (subir no GitHub Release):"
    Write-Host "    $(Join-Path $rel "SPHONE-Setup-$version.exe")"
    Write-Host "    $(Join-Path $rel 'sphone-version.json')"
}

if ($Run) {
    Write-Host "`nAbrindo build dev..." -ForegroundColor Cyan
    Start-Process $devExe
}
