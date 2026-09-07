# Assemble PdfToMarkdown.app skeleton on Windows (no Mach-O CLI).
# Full runnable bundle requires scripts/build_macos.sh on Apple Silicon.
param(
    [string]$Version = "1.0.0",
    [string]$ModelsSrc = "D:/Environment/PaddleOCR-models/PP-OCRv6"
)
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$Out = Join-Path $Root "release\PdfToMarkdown-$Version-macos-arm64"
$App = Join-Path $Out "PdfToMarkdown.app"
$MacOS = Join-Path $App "Contents\MacOS"
$Fw = Join-Path $App "Contents\Frameworks"
$Res = Join-Path $App "Contents\Resources"
if (Test-Path $Out) { Remove-Item -Recurse -Force $Out }
New-Item -ItemType Directory -Force -Path $MacOS, $Fw, (Join-Path $Res "models"), (Join-Path $Res "config") | Out-Null

Copy-Item (Join-Path $Root "app\macos\Info.plist") (Join-Path $App "Contents\Info.plist")
Set-Content -Path (Join-Path $App "Contents\PkgInfo") -Value "APPL????" -NoNewline -Encoding ascii
Copy-Item (Join-Path $Root "app\macos\launcher.sh") (Join-Path $MacOS "PdfToMarkdown")
Copy-Item (Join-Path $Root "config\pdf_to_md.json") (Join-Path $Res "config\")
Copy-Item (Join-Path $Root "docs\USER_GUIDE.md") $Res -ErrorAction SilentlyContinue
Copy-Item (Join-Path $Root "README.md") $Out -ErrorAction SilentlyContinue

function Copy-IfExists($src, $dstDir) {
    if (Test-Path $src) { Copy-Item $src $dstDir }
}
Copy-IfExists (Join-Path $Root "third_party\pdfium-macos\lib\libpdfium.dylib") $Fw
$paddleLib = Join-Path $Root "third_party\paddle_inference_macos\paddle\lib"
if (Test-Path $paddleLib) {
    Get-ChildItem $paddleLib -Filter "*.dylib" | ForEach-Object { Copy-Item $_.FullName $Fw }
}
$ocvLib = Join-Path $Root "third_party\opencv-macos\lib"
if (Test-Path $ocvLib) {
    Get-ChildItem $ocvLib -Filter "*.dylib" | ForEach-Object { Copy-Item $_.FullName $Fw }
}

if (Test-Path (Join-Path $ModelsSrc "PP-OCRv6_small_det")) {
    Copy-Item -Recurse (Join-Path $ModelsSrc "PP-OCRv6_small_det") (Join-Path $Res "models\PP-OCRv6_small_det")
    Copy-Item -Recurse (Join-Path $ModelsSrc "PP-OCRv6_small_rec") (Join-Path $Res "models\PP-OCRv6_small_rec")
}

@"
PdfToMarkdown $Version
Platform: macOS arm64 (Apple Silicon) — SKELETON from Windows
This folder has dylibs + launcher + models but NOT PdfToMarkdown.Cli.

On a Mac:
  ./scripts/fetch_macos_deps.sh   # skip if third_party already filled
  ./scripts/build_macos.sh
"@ | Set-Content (Join-Path $Out "VERSION.txt") -Encoding UTF8

$Zip = Join-Path $Root "release\PdfToMarkdown-$Version-macos-arm64.zip"
if (Test-Path $Zip) { Remove-Item $Zip -Force }
Compress-Archive -Path $Out -DestinationPath $Zip -Force
Write-Host "Created $App (skeleton; compile CLI on Mac)" -ForegroundColor Yellow
Write-Host "Created $Zip" -ForegroundColor Green
