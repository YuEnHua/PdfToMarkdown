# Package PdfToMarkdown Win11 x64 portable release
param(
    [string]$BuildDir = ".\build",
    [string]$OutputDir = ".\release",
    [string]$Version = "1.0.0",
    [string]$Config = "Release",
    [string]$ModelsSrc = "D:/Environment/PaddleOCR-models/PP-OCRv6"
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot

$PackageName = "PdfToMarkdown-$Version-windows-x64"
$PackageDir = Join-Path $Root "$OutputDir\$PackageName"
if (Test-Path $PackageDir) { Remove-Item -Recurse -Force $PackageDir }
New-Item -ItemType Directory -Force -Path $PackageDir | Out-Null

$BinCandidates = @(
    (Join-Path $Root "$BuildDir\bin\$Config"),
    (Join-Path $Root "$BuildDir\bin")
)
$BinDir = $BinCandidates | Where-Object { Test-Path (Join-Path $_ "PdfToMarkdown.Native.dll") } | Select-Object -First 1
if (-not $BinDir) { throw "PdfToMarkdown.Native.dll not found. Build first with scripts/build_all.ps1" }

Write-Host "Packaging from $BinDir" -ForegroundColor Cyan
$DestBin = Join-Path $PackageDir "bin"
New-Item -ItemType Directory -Force -Path $DestBin | Out-Null

$names = @(
    "PdfToMarkdown.Native.dll",
    "PdfToMarkdown.Cli.exe",
    "PdfToMarkdown.Gui.exe",
    "pdfium.dll",
    "polyclipping.dll",
    "paddle_inference.dll",
    "common.dll",
    "phi.dll",
    "mklml.dll",
    "libiomp5md.dll"
)
foreach ($n in $names) {
    $p = Join-Path $BinDir $n
    if (Test-Path $p) { Copy-Item $p $DestBin }
}
Get-ChildItem $BinDir -Filter "opencv_world*.dll" |
    Where-Object { $_.Name -notmatch 'd\.dll$' } |
    ForEach-Object { Copy-Item $_.FullName $DestBin }

Copy-Item -Recurse (Join-Path $Root "config") (Join-Path $PackageDir "config")
Copy-Item -Recurse (Join-Path $Root "docs") (Join-Path $PackageDir "docs") -ErrorAction SilentlyContinue
Copy-Item (Join-Path $Root "README.md") $PackageDir -ErrorAction SilentlyContinue

$Lic = Join-Path $PackageDir "licenses"
New-Item -ItemType Directory -Force -Path $Lic | Out-Null
Copy-Item -Recurse (Join-Path $Root "third_party\pdfium\licenses") (Join-Path $Lic "pdfium") -ErrorAction SilentlyContinue
Copy-Item (Join-Path $Root "third_party\pdfium\LICENSE") (Join-Path $Lic "pdfium-LICENSE.txt") -ErrorAction SilentlyContinue
Copy-Item (Join-Path $Root "THIRD_PARTY_LICENSES.md") $PackageDir -ErrorAction SilentlyContinue

$ModelsDst = Join-Path $PackageDir "models"
New-Item -ItemType Directory -Force -Path $ModelsDst | Out-Null
if (Test-Path $ModelsSrc) {
    foreach ($name in @("PP-OCRv6_small_det", "PP-OCRv6_small_rec")) {
        Copy-Item -Recurse (Join-Path $ModelsSrc $name) (Join-Path $ModelsDst $name)
    }
}

$VersionTxt = @"
PdfToMarkdown $Version
Platform: Windows 11 x64
Build: $(Get-Date -Format "yyyy-MM-dd")

Run:
  bin\PdfToMarkdown.Gui.exe
  bin\PdfToMarkdown.Cli.exe input.pdf -o out.md --models ..\models --dpi 200

Offline. No network required at runtime.
"@
Set-Content -Path (Join-Path $PackageDir "VERSION.txt") -Value $VersionTxt -Encoding UTF8

$Zip = Join-Path $Root "$OutputDir\$PackageName.zip"
if (Test-Path $Zip) { Remove-Item $Zip -Force }
Compress-Archive -Path $PackageDir -DestinationPath $Zip -Force
Write-Host "Created $PackageDir" -ForegroundColor Green
Write-Host "Created $Zip" -ForegroundColor Green
