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
ARGS=("$@")
if command -v brew >/dev/null 2>&1; then
  _ocv="$(brew --prefix opencv 2>/dev/null || true)"
  if [[ -n "$_ocv" && -f "$_ocv/lib/cmake/opencv4/OpenCVConfig.cmake" ]]; then
    ARGS+=(--skip-opencv)
  fi
fi
exec "$PY" "$ROOT/scripts/fetch_macos_deps.py" "${ARGS[@]}"
