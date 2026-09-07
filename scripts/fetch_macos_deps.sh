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
  for formula in opencv@4 opencv; do
    _ocv="$(brew --prefix "$formula" 2>/dev/null || true)"
    for d in "$_ocv/lib/cmake/opencv4" "$_ocv/lib/cmake/opencv5"; do
      if [[ -n "$_ocv" && -f "$d/OpenCVConfig.cmake" ]]; then
        ARGS+=(--skip-opencv)
        break 2
      fi
    done
  done
fi
exec "$PY" "$ROOT/scripts/fetch_macos_deps.py" "${ARGS[@]}"
