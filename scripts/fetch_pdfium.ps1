# Fetch PDFium Windows x64 binaries (bblanchon/pdfium-binaries)
param(
    [string]$VersionTag = "chromium/7961",
    [string]$Dest = ""
)
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
if (-not $Dest) { $Dest = Join-Path $Root "third_party\pdfium" }
New-Item -ItemType Directory -Force -Path $Dest | Out-Null
if (Test-Path (Join-Path $Dest "include\fpdfview.h")) {
    Write-Host "PDFium already present at $Dest"
    exit 0
}
$tagEnc = [uri]::EscapeDataString($VersionTag).Replace("%2F", "%2F")
$url = "https://github.com/bblanchon/pdfium-binaries/releases/download/chromium%2F7961/pdfium-win-x64.tgz"
$zip = Join-Path $env:TEMP "pdfium-win-x64.tgz"
Write-Host "Downloading $url"
curl.exe -L --fail -o $zip $url
tar -xzf $zip -C $Dest
Write-Host "PDFium extracted to $Dest"
