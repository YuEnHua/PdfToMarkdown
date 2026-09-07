#!/usr/bin/env bash
# Assemble a portable PdfToMarkdown.app (no Homebrew, no install).
# Must run on macOS after build-macos. On Windows use package_macos.ps1 for skeleton only.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VERSION="1.0.0"
OUT="${ROOT}/release/PdfToMarkdown-${VERSION}-macos-arm64"
APP="${OUT}/PdfToMarkdown.app"
MACOS="${APP}/Contents/MacOS"
FW="${APP}/Contents/Frameworks"
RES="${APP}/Contents/Resources"

BIN=""
for cand in \
  "${ROOT}/build-macos/bin/PdfToMarkdown.Cli" \
  "${ROOT}/build-macos/bin/Release/PdfToMarkdown.Cli"; do
  if [[ -f "$cand" ]]; then BIN="$(dirname "$cand")"; break; fi
done

rm -rf "$OUT"
mkdir -p "$MACOS" "$FW" "$RES/models" "$RES/config"

cp "${ROOT}/app/macos/Info.plist" "${APP}/Contents/Info.plist"
echo -n "APPL????" > "${APP}/Contents/PkgInfo"
cp "${ROOT}/app/macos/launcher.sh" "${MACOS}/PdfToMarkdown"
chmod +x "${MACOS}/PdfToMarkdown"

if [[ -n "$BIN" ]]; then
  cp "${BIN}/PdfToMarkdown.Cli" "$MACOS/"
  chmod +x "${MACOS}/PdfToMarkdown.Cli"
  shopt -s nullglob
  for f in "${BIN}"/*.dylib "${BIN}"/PdfToMarkdown.Native.dylib; do
    [[ -f "$f" ]] && cp "$f" "$FW/"
  done
  shopt -u nullglob
fi

# Always stage downloaded prebuilts so rpath neighbors exist.
[[ -f "${ROOT}/third_party/pdfium-macos/lib/libpdfium.dylib" ]] && \
  cp "${ROOT}/third_party/pdfium-macos/lib/libpdfium.dylib" "$FW/"
if [[ -d "${ROOT}/third_party/paddle_inference_macos/paddle/lib" ]]; then
  cp "${ROOT}/third_party/paddle_inference_macos/paddle/lib/"*.dylib "$FW/" 2>/dev/null || true
fi
if [[ -d "${ROOT}/third_party/opencv-macos/lib" ]]; then
  cp "${ROOT}/third_party/opencv-macos/lib/"*.dylib "$FW/" 2>/dev/null || true
fi

cp "${ROOT}/config/pdf_to_md.json" "${RES}/config/"
cp "${ROOT}/README.md" "$OUT/" 2>/dev/null || true
cp "${ROOT}/docs/USER_GUIDE.md" "$RES/" 2>/dev/null || true

MODELS_SRC="${MODELS_SRC:-}"
if [[ -z "$MODELS_SRC" ]]; then
  for m in \
    "${BIN:-/nonexistent}/models" \
    "/Environment/PaddleOCR-models/PP-OCRv6" \
    "$HOME/PaddleOCR-models/PP-OCRv6" \
    "${ROOT}/../MedicalOCR/models"; do
    if [[ -d "${m}/PP-OCRv6_small_det" ]]; then MODELS_SRC="$m"; break; fi
  done
fi
if [[ -n "$MODELS_SRC" ]]; then
  cp -R "${MODELS_SRC}/PP-OCRv6_small_det" "${RES}/models/"
  cp -R "${MODELS_SRC}/PP-OCRv6_small_rec" "${RES}/models/"
elif [[ ! -d "${RES}/models/PP-OCRv6_small_det" ]]; then
  echo "ERROR: OCR models not found. Set MODELS_SRC to PP-OCRv6 root (det+rec)." >&2
  exit 1
fi

if command -v install_name_tool >/dev/null 2>&1; then
  shopt -s nullglob
  for bin in "${MACOS}/PdfToMarkdown.Cli" "${FW}"/*.dylib; do
    [[ -f "$bin" ]] || continue
    install_name_tool -add_rpath "@loader_path/../Frameworks" "$bin" 2>/dev/null || true
    install_name_tool -add_rpath "@loader_path" "$bin" 2>/dev/null || true
  done
  shopt -u nullglob
fi

cat > "${OUT}/VERSION.txt" <<EOF
PdfToMarkdown ${VERSION}
Platform: macOS arm64 (Apple Silicon)
Offline. No Homebrew install required.

Run:
  open PdfToMarkdown.app
  PdfToMarkdown.app/Contents/MacOS/PdfToMarkdown.Cli scan.pdf --models PdfToMarkdown.app/Contents/Resources/models

Gatekeeper: first launch may need Right-click -> Open.
EOF

if [[ -z "$BIN" ]]; then
  echo "WARNING: PdfToMarkdown.Cli not found. Skeleton .app created; compile on a Mac with scripts/build_macos.sh" >&2
fi

(cd "$(dirname "$OUT")" && zip -r -q "$(basename "$OUT").zip" "$(basename "$OUT")") || true
echo "Created $APP"
echo "Created ${OUT}.zip"
