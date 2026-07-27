# Assert blank-line sample produces underscores in Markdown
param(
    [string]$BinDir = "..\build\bin\Release",
    [string]$Models = "D:\Environment\PaddleOCR-models\PP-OCRv6"
)
$ErrorActionPreference = "Continue"
$Root = Split-Path -Parent $PSScriptRoot
$pdf = Join-Path $Root "samples\sample_blanks_worksheet.pdf"
$out = Join-Path $Root "samples\sample_blanks_worksheet.out.md"
$cli = Join-Path $BinDir "PdfToMarkdown.Cli.exe"
if (-not (Test-Path $cli)) { throw "CLI not found: $cli" }
if (-not (Test-Path $pdf)) { throw "Sample PDF missing: $pdf" }

& $cli $pdf -o $out --models $Models --dpi 200 2>$null
$rc = $LASTEXITCODE
$ErrorActionPreference = "Stop"
if ($rc -ne 0) { throw "CLI failed: $rc" }

$md = Get-Content $out -Raw -Encoding UTF8
if ($md -notmatch "_{3,}") {
    Write-Host "FAIL: expected ____ fill-in blanks in markdown"
    Write-Host $md
    exit 1
}
if ($md -notmatch "There" -and $md -notmatch "wrong" -and $md -notmatch "老师") {
    Write-Host "WARN: surrounding OCR text weak, but blanks present"
}
Write-Host "PASS: blank underscores found in markdown"
Write-Host $md
