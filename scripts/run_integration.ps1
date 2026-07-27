# Integration check helper
param(
    [string]$BinDir = "..\build\bin\Release",
    [string]$Models = "D:\Environment\PaddleOCR-models\PP-OCRv6"
)
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$pdf = Join-Path $Root "samples\sample_a4_scan.pdf"
$out = Join-Path $Root "samples\sample_a4_scan.out.md"
$cli = Join-Path $BinDir "PdfToMarkdown.Cli.exe"
if (-not (Test-Path $cli)) { throw "CLI not found: $cli" }

& $cli $pdf -o $out --models $Models --dpi 200
if ($LASTEXITCODE -ne 0) { throw "CLI failed: $LASTEXITCODE" }

$actual = Get-Content $out -Raw -Encoding UTF8
if ($actual -notmatch "Hospital Sample Report" -or $actual -notmatch "Zhang San") {
    Write-Host "FAIL: expected OCR phrases missing"
    Write-Host $actual
    exit 1
}
Write-Host "PASS: integration OCR content present"
Write-Host $actual
