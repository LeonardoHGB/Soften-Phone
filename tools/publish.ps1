<#
  publish.ps1 — Gera a pasta de distribuicao (dist) do SPHONE: compila Release,
  copia o exe, roda o windeployqt (DLLs/plugins do Qt) e garante o runtime VC++
  (/MD) app-local. Depois o setup.iss (Inno Setup) empacota a dist num instalador.

  Uso: powershell -NoProfile -ExecutionPolicy Bypass -File tools\publish.ps1
       ... -NoBuild   (pula a compilacao, so re-stagea)
#>
[CmdletBinding()]
param(
    [string] $QtDir = 'C:\Qt\6.8.3\msvc2022_64',
    [switch] $NoBuild
)
$ErrorActionPreference = 'Stop'

$root  = Split-Path -Parent $PSScriptRoot
$build = Join-Path $root 'build'
$dist  = Join-Path $root 'dist'

if (-not $NoBuild) {
    & (Join-Path $PSScriptRoot 'build.ps1') -WithPjsip -QtDir $QtDir
    if ($LASTEXITCODE -ne 0) { throw "build falhou ($LASTEXITCODE)" }
}
if (-not (Test-Path (Join-Path $build 'SPHONE.exe'))) { throw "SPHONE.exe nao encontrado em $build" }

# Staging limpo.
if (Test-Path $dist) { Remove-Item -Recurse -Force $dist }
New-Item -ItemType Directory -Force $dist | Out-Null
Copy-Item (Join-Path $build 'SPHONE.exe') $dist

# DLLs + plugins do Qt (versao Release, sem traducoes).
$windeploy = Join-Path $QtDir 'bin\windeployqt.exe'
if (-not (Test-Path $windeploy)) { throw "windeployqt nao encontrado em $windeploy" }
Write-Host '==> windeployqt...' -ForegroundColor Cyan
& $windeploy --release --no-translations --no-system-d3d-compiler --dir $dist (Join-Path $dist 'SPHONE.exe')
if ($LASTEXITCODE -ne 0) { throw "windeployqt falhou ($LASTEXITCODE)" }

# Runtime VC++ (/MD) app-local — do System32 (a instalacao do VS o coloca la).
foreach ($d in 'vcruntime140.dll', 'vcruntime140_1.dll', 'msvcp140.dll') {
    if (-not (Test-Path (Join-Path $dist $d))) {
        $src = Join-Path $env:SystemRoot "System32\$d"
        if (Test-Path $src) { Copy-Item $src $dist; Write-Host "  + $d" }
        else { Write-Host "  !! $d nao encontrado (cliente precisa do VC++ Redistributable)" -ForegroundColor Yellow }
    }
}

Write-Host "dist pronto: $dist" -ForegroundColor Green
"{0:N1} MB em {1} arquivos" -f (((Get-ChildItem $dist -Recurse -File | Measure-Object Length -Sum).Sum)/1MB),
    (Get-ChildItem $dist -Recurse -File).Count
