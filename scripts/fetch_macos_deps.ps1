# Download Apple Silicon prebuilts into third_party/ (does not touch Windows pdfium)
param()
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$py = Get-Command python -ErrorAction SilentlyContinue
if (-not $py) { $py = Get-Command python3 -ErrorAction SilentlyContinue }
if (-not $py) { throw "python/python3 required to fetch Mac deps" }
& $py.Source (Join-Path $PSScriptRoot "fetch_macos_deps.py")
if ($LASTEXITCODE -ne 0) { throw "fetch_macos_deps.py failed" }
