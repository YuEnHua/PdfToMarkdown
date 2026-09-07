#!/usr/bin/env bash
# Fetch deps, build CLI, package portable .app (Apple Silicon only).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

if [[ "$(uname -s)" != "Darwin" ]]; then
  echo "build_macos.sh must run on macOS. On Windows: python scripts/fetch_macos_deps.py && powershell scripts/package_macos.ps1" >&2
  exit 1
fi
arch="$(uname -m)"
if [[ "$arch" != "arm64" ]]; then
  echo "This build is Apple Silicon (arm64) only. Current: $arch" >&2
  exit 1
fi

"$ROOT/scripts/fetch_macos_deps.sh"

OCV_DIR="$ROOT/third_party/opencv-macos/lib/cmake/opencv4"
if command -v brew >/dev/null 2>&1; then
  for formula in opencv@4 opencv; do
    BREW_OCV="$(brew --prefix "$formula" 2>/dev/null || true)"
    for d in "$BREW_OCV/lib/cmake/opencv4" "$BREW_OCV/lib/cmake/opencv5"; do
      if [[ -n "$BREW_OCV" && -f "$d/OpenCVConfig.cmake" ]]; then
        OCV_DIR="$d"
        break 2
      fi
    done
  done
fi

cmake -S "$ROOT" -B "$ROOT/build-macos" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DPDF_TO_MD_STATIC_RUNTIME=OFF \
  -DPADDLE_INFERENCE_DIR="$ROOT/third_party/paddle_inference_macos" \
  -DOpenCV_DIR="$OCV_DIR"

cmake --build "$ROOT/build-macos" --config Release --target PdfToMarkdown.Native PdfToMarkdown.Cli PdfToMarkdown.Tests -j

"$ROOT/scripts/package_macos.sh"
echo "Build output: $ROOT/build-macos/bin"
echo "App: $ROOT/release/PdfToMarkdown-1.0.0-macos-arm64/PdfToMarkdown.app"
