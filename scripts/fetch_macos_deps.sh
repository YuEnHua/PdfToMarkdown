#!/usr/bin/env bash
# Download Apple Silicon prebuilts into third_party/ (does not touch Windows pdfium)
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
if command -v python3 >/dev/null 2>&1; then
  PY=python3
elif command -v python >/dev/null 2>&1; then
  PY=python
else
  echo "python3 required" >&2
  exit 1
fi
exec "$PY" "$ROOT/scripts/fetch_macos_deps.py"
