<#
  build-pjsip-md.ps1 — Compila o PJSIP (pjsua-lib + deps) com runtime DINAMICO
  (/MD) para casar com o Qt e poder linkar estaticamente no SPHONE.exe.
  Espelha as opcoes do build-x64 (/MT) ja existente: Ninja, Release, WASAPI off,
  video on, todas as deps bundled. Saida em C:\src\pjproject\build-md.
#>
[CmdletBinding()]
param(
    [string] $Pj    = 'C:\src\pjproject',
    [string] $Build = 'C:\src\pjproject\build-md'
)
$ErrorActionPreference = 'Stop'

$vsw = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs  = & $vsw -latest -property installationPath
$cmake  = Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$ninja  = Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe'
$vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'

# Importa o ambiente do MSVC (cl/link/INCLUDE/LIB).
cmd /c "`"$vcvars`" >nul 2>&1 && set" | ForEach-Object {
    if ($_ -match '^(.*?)=(.*)$') { Set-Item -Path "Env:$($Matches[1])" -Value $Matches[2] }
}

$cfg = @(
    '-S', $Pj, '-B', $Build, '-G', 'Ninja',
    '-DCMAKE_BUILD_TYPE=Release',
    '-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL',
    "-DCMAKE_MAKE_PROGRAM=$ninja",
    '-DPJ_SKIP_EXPERIMENTAL_NOTICE=ON',
    '-DPJMEDIA_WITH_AUDIODEV_WASAPI=OFF',
    '-DPJMEDIA_WITH_VIDEODEV_DSHOW=OFF',
    '-DPJMEDIA_WITH_VIDEODEV_SDL=OFF',
    '-DPJ_DEP_G7221=bundled', '-DPJ_DEP_GSM=bundled', '-DPJ_DEP_ILBC=bundled',
    '-DPJ_DEP_RESAMPLE=bundled', '-DPJ_DEP_SPEEX=bundled', '-DPJ_DEP_SRTP=bundled',
    '-DPJ_DEP_WEBRTC=bundled', '-DPJ_DEP_WEBRTC_AEC3=bundled', '-DPJ_DEP_YUV=bundled'
)
Write-Host '==> Configurando build-md (/MD)...' -ForegroundColor Cyan
& $cmake @cfg
if ($LASTEXITCODE -ne 0) { throw "configure falhou ($LASTEXITCODE)" }

Write-Host '==> Compilando pjsua-lib (+ deps) em /MD...' -ForegroundColor Cyan
& $cmake --build $Build --target pjsua-lib
if ($LASTEXITCODE -ne 0) { throw "build falhou ($LASTEXITCODE)" }

Write-Host 'OK: PJSIP /MD pronto em ' -NoNewline; Write-Host $Build -ForegroundColor Green
