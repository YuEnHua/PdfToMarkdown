# Benchmark notes (local Win11 x64, CPU)

Date: 2026-07-25
Machine: Windows 11, Paddle Inference 3.3.0 CPU+MKL, PP-OCRv6 Small, DPI=200, threads=8

| Sample | Pages | Wall time | Notes |
|--------|-------|-----------|-------|
| sample_a4_scan.pdf | 1 | ~7.4 s | includes model load on first Create |

Observations:
- First Convert after Create pays model-load cost (~seconds).
- Subsequent pages on a warm engine are dominated by det+rec.
- Prefer DPI 150–200 for long documents; 300 increases render+OCR cost.
- Serial page OCR keeps memory stable; parallel pages reserved for later.

# How to re-run:
```powershell
$bin = "..\build\bin\Release"
Measure-Command {
  & "$bin\PdfToMarkdown.Cli.exe" samples\sample_a4_scan.pdf -o samples\bench.md `
    --models D:\Environment\PaddleOCR-models\PP-OCRv6 --dpi 200
}
```
