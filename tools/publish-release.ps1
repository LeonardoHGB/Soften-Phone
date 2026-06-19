<#
  publish-release.ps1 — Gera um RELEASE completo do SPHONE para o auto-update:
    1) build + dist (publish.ps1)
    2) instalador Inno  -> packaging\Output\SPHONE-Setup-<versao>.exe
    3) sphone-version.json (versao + url + sha256 + notes)

  Depois, suba OS DOIS arquivos (SPHONE-Setup-<versao>.exe e sphone-version.json)
  como assets de um GitHub Release em LeonardoHGB/SPhone. O app instalado checa o
  sphone-version.json do 'latest' e se atualiza sozinho.

  A versao sai do src\core\version.h (fonte unica). Bump dela + a FILEVERSION do
  app.rc antes de lancar. Uso: tools\publish-release.ps1 [-Notes "..."] [-NoBuild]
#>
[CmdletBinding()]
param(
    [string] $Notes = 'Atualizacao do Soften Phone.',
    [switch] $NoBuild
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot

# Versao do version.h.
$vh = Get-Content (Join-Path $root 'src\core\version.h') -Raw
if ($vh -notmatch 'SPHONE_VERSION\s+"([^"]+)"') { throw 'Nao consegui ler SPHONE_VERSION de src\core\version.h' }
$version = $Matches[1]
Write-Host "==> Release versao $version" -ForegroundColor Cyan

# 1) build + dist
if (-not $NoBuild) {
    & (Join-Path $PSScriptRoot 'publish.ps1')
    if ($LASTEXITCODE -ne 0) { throw "publish falhou ($LASTEXITCODE)" }
}

# 2) instalador (passa a versao para o setup.iss).
$iscc = "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe"
if (-not (Test-Path $iscc)) {
    $iscc = (Get-ChildItem "$env:LOCALAPPDATA\Programs","${env:ProgramFiles(x86)}","$env:ProgramFiles" `
              -Recurse -Filter 'ISCC.exe' -ErrorAction SilentlyContinue | Select-Object -First 1).FullName
}
if (-not $iscc) { throw 'ISCC.exe (Inno Setup) nao encontrado' }
& $iscc "/DMyAppVersion=$version" (Join-Path $root 'packaging\setup.iss')
if ($LASTEXITCODE -ne 0) { throw "ISCC falhou ($LASTEXITCODE)" }

$setup = Join-Path $root "packaging\Output\SPHONE-Setup-$version.exe"
if (-not (Test-Path $setup)) { throw "Instalador nao encontrado: $setup" }

# 3) sphone-version.json
$sha = (Get-FileHash $setup -Algorithm SHA256).Hash
$manifest = [ordered]@{
    version = $version
    url     = "https://github.com/LeonardoHGB/SPhone/releases/latest/download/SPHONE-Setup-$version.exe"
    sha256  = $sha
    notes   = $Notes
}
$jsonPath = Join-Path $root 'packaging\Output\sphone-version.json'
($manifest | ConvertTo-Json) | Set-Content -Path $jsonPath -Encoding utf8

Write-Host ''
Write-Host 'Release pronto. Suba estes dois assets no GitHub Release:' -ForegroundColor Green
Write-Host "  $setup"
Write-Host "  $jsonPath"
Write-Host "sha256: $sha"
