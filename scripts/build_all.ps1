# PdfToMarkdown — standalone build
param(
    [string]$BuildDir = ".\build",
    [string]$Config = "Release",
    [string]$PaddleDir = "D:/Environment/paddle_inference_3.3.0",
    [string]$OpenCvDir = "D:/Environment/opencv/build",
    [string]$ModelsSrc = "D:/Environment/PaddleOCR-models/PP-OCRv6",
    [string]$MedicalOcrRoot = ""
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
if (-not $MedicalOcrRoot) {
    $MedicalOcrRoot = Join-Path (Split-Path -Parent $Root) "MedicalOCR"
}

Write-Host "Configuring standalone PdfToMarkdown..." -ForegroundColor Cyan
Write-Host "  Root:        $Root"
Write-Host "  MedicalOCR:  $MedicalOcrRoot"

$cmake = "cmake"
$cmakeVs = "D:\Environment\VS2022BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
if (Test-Path $cmakeVs) { $cmake = $cmakeVs }

$json = Join-Path $MedicalOcrRoot "build\_deps\nlohmann_json-src"
$gtest = Join-Path $MedicalOcrRoot "build\_deps\googletest-src"
$extra = @("-DMEDICAL_OCR_ROOT=$MedicalOcrRoot")
if (Test-Path (Join-Path $json "include\nlohmann\json.hpp")) {
    $extra += "-DFETCHCONTENT_SOURCE_DIR_NLOHMANN_JSON=$json"
}
if (Test-Path (Join-Path $gtest "googletest\include\gtest\gtest.h")) {
    $extra += "-DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=$gtest"
}

$buildPath = Join-Path $Root $BuildDir
& $cmake -S $Root -B $buildPath -G "Visual Studio 17 2022" -A x64 `
    -DPDF_TO_MD_STATIC_RUNTIME=ON `
    -DPADDLE_INFERENCE_DIR="$PaddleDir" `
    -DOpenCV_DIR="$OpenCvDir" `
    @extra

if ($LASTEXITCODE -ne 0) { throw "CMake configure failed" }

& $cmake --build $buildPath --config $Config `
    --target PdfToMarkdown.Native PdfToMarkdown.Cli PdfToMarkdown.Gui PdfToMarkdown.Tests
if ($LASTEXITCODE -ne 0) { throw "CMake build failed" }

$bin = Join-Path $buildPath "bin\$Config"
if (-not (Test-Path $bin)) { $bin = Join-Path $buildPath "bin" }

$modelsDst = Join-Path $bin "models"
New-Item -ItemType Directory -Force -Path $modelsDst | Out-Null
if (Test-Path $ModelsSrc) {
    foreach ($name in @("PP-OCRv6_small_det", "PP-OCRv6_small_rec")) {
        $src = Join-Path $ModelsSrc $name
        $dst = Join-Path $modelsDst $name
        if (-not (Test-Path $dst)) {
            cmd /c "mklink /J `"$dst`" `"$src`"" | Out-Null
            if (-not (Test-Path $dst)) { Copy-Item -Recurse $src $dst }
        }
    }
}

Write-Host "Build output: $bin" -ForegroundColor Green
Write-Host "  PdfToMarkdown.Gui.exe"
Write-Host "  PdfToMarkdown.Cli.exe"
Write-Host "  PdfToMarkdown.Native.dll"
