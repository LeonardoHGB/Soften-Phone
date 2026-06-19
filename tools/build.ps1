<#
  build.ps1 — Configura e compila o SPHONE com o MSVC do VS 2026 + CMake/Ninja
  embutidos no VS, importando o ambiente do vcvars64 (cl.exe/rc.exe no PATH).

  Uso:
    powershell -NoProfile -ExecutionPolicy Bypass -File tools\build.ps1
    ... -WithPjsip      (liga SPHONE_WITH_PJSIP=ON; requer PJSIP buildado /MD)
    ... -QtDir 'C:\Qt\6.8.3\msvc2022_64'
    ... -Run            (executa o SPHONE.exe ao final)
#>
[CmdletBinding()]
param(
    [string] $QtDir = 'C:\Qt\6.8.3\msvc2022_64',
    [switch] $WithPjsip,
    [switch] $Run,
    [switch] $Clean
)
$ErrorActionPreference = 'Stop'

$root  = Split-Path -Parent $PSScriptRoot          # ...\SPHONE
$build = Join-Path $root 'build'

$vsw = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs  = & $vsw -latest -property installationPath
if (-not $vs) { throw 'Visual Studio nao encontrado (vswhere).' }
$cmake  = Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$ninja  = Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe'
$vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'
foreach ($p in @($cmake,$ninja,$vcvars)) { if (-not (Test-Path $p)) { throw "Nao encontrado: $p" } }
if (-not (Test-Path (Join-Path $QtDir 'bin\qmake.exe'))) { throw "Qt nao encontrado em $QtDir (rode o bootstrap)." }

# Importa o ambiente do vcvars64 (cl, rc, link, INCLUDE/LIB) para esta sessao.
cmd /c "`"$vcvars`" >nul 2>&1 && set" | ForEach-Object {
    if ($_ -match '^(.*?)=(.*)$') { Set-Item -Path "Env:$($Matches[1])" -Value $Matches[2] }
}

if ($Clean -and (Test-Path $build)) { Remove-Item -Recurse -Force $build }

$pjsip = if ($WithPjsip) { 'ON' } else { 'OFF' }
Write-Host "==> Configurando (SPHONE_WITH_PJSIP=$pjsip, Qt=$QtDir)" -ForegroundColor Cyan
# Args num array com strings expandidas (evita bareword nao-expandido tipo "$pjsip").
$cfgArgs = @(
    '-S', $root, '-B', $build, '-G', 'Ninja',
    '-DCMAKE_BUILD_TYPE=Release',
    "-DCMAKE_MAKE_PROGRAM=$ninja",
    "-DCMAKE_PREFIX_PATH=$QtDir",
    "-DSPHONE_WITH_PJSIP=$pjsip"
)
& $cmake @cfgArgs
if ($LASTEXITCODE -ne 0) { throw "Falha no configure (exit $LASTEXITCODE)." }

Write-Host "==> Compilando" -ForegroundColor Cyan
& $cmake --build $build
if ($LASTEXITCODE -ne 0) { throw "Falha no build (exit $LASTEXITCODE)." }

$exe = Join-Path $build 'SPHONE.exe'
Write-Host "OK -> $exe" -ForegroundColor Green
if ($Run) { & $exe }
