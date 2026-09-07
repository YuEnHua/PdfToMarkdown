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
    [[ -f "$f" ]] && cp -L "$f" "$FW/"
  done
  shopt -u nullglob
fi

[[ -f "${ROOT}/third_party/pdfium-macos/lib/libpdfium.dylib" ]] && \
  cp -L "${ROOT}/third_party/pdfium-macos/lib/libpdfium.dylib" "$FW/"
if [[ -d "${ROOT}/third_party/paddle_inference_macos/paddle/lib" ]]; then
  shopt -s nullglob
  for f in "${ROOT}/third_party/paddle_inference_macos/paddle/lib/"*.dylib; do
    cp -L "$f" "$FW/"
  done
  shopt -u nullglob
fi

SEARCH_LIBS=(
  "${BIN:-}"
  "${ROOT}/third_party/opencv-macos/lib"
  "${ROOT}/third_party/paddle_inference_macos/paddle/lib"
  "${ROOT}/third_party/pdfium-macos/lib"
)

find_src() {
  local want="$1"
  local d stem hit
  for d in "${SEARCH_LIBS[@]}"; do
    [[ -n "$d" && -d "$d" ]] || continue
    if [[ -e "$d/$want" ]]; then
      echo "$d/$want"
      return 0
    fi
  done
  stem="${want%.dylib}"
  stem="${stem%.*}"
  for d in "${SEARCH_LIBS[@]}"; do
    [[ -n "$d" && -d "$d" ]] || continue
    shopt -s nullglob
    for hit in "$d/${stem}".dylib "$d/${stem}".*.dylib; do
      if [[ -e "$hit" ]]; then
        echo "$hit"
        shopt -u nullglob
        return 0
      fi
    done
    shopt -u nullglob
  done
  return 1
}

is_system_dep() {
  case "$1" in
    /usr/lib/*|/System/*) return 0 ;;
  esac
  return 1
}

# Pull the transitive dylib closure instead of globbing unused OpenCV contrib.
if command -v otool >/dev/null 2>&1; then
  changed=1
  while [[ "$changed" == 1 ]]; do
    changed=0
    shopt -s nullglob
    for bin in "${MACOS}/PdfToMarkdown.Cli" "${FW}"/*.dylib; do
      [[ -f "$bin" ]] || continue
      while IFS= read -r dep; do
        [[ -z "$dep" ]] && continue
        is_system_dep "$dep" && continue
        n="$(basename "$dep")"
        [[ "$n" == "libc++.1.dylib" ]] && continue
        if [[ ! -e "$FW/$n" ]]; then
          if src="$(find_src "$n")"; then
            echo "  copy $(basename "$src") -> Frameworks/$n"
            cp -L "$src" "$FW/$n"
            changed=1
          else
            echo "WARNING: missing $n (needed by $(basename "$bin"))" >&2
          fi
        fi
      done < <(otool -L "$bin" | awk '/^\t/ {print $1}')
    done
    shopt -u nullglob
  done
elif [[ -d "${ROOT}/third_party/opencv-macos/lib" ]]; then
  shopt -s nullglob
  for f in "${ROOT}/third_party/opencv-macos/lib/"*.dylib; do
    cp -L "$f" "$FW/"
  done
  shopt -u nullglob
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
    if [[ "$bin" == *.dylib ]]; then
      install_name_tool -id "@rpath/$(basename "$bin")" "$bin" 2>/dev/null || true
    fi
    install_name_tool -add_rpath "@loader_path/../Frameworks" "$bin" 2>/dev/null || true
    install_name_tool -add_rpath "@loader_path" "$bin" 2>/dev/null || true
    # Conda OpenCV links libc++ via @rpath; macOS provides it at /usr/lib.
    install_name_tool -change "@rpath/libc++.1.dylib" "/usr/lib/libc++.1.dylib" "$bin" 2>/dev/null || true
    if command -v otool >/dev/null 2>&1; then
      while IFS= read -r dep; do
        [[ -z "$dep" ]] && continue
        n="$(basename "$dep")"
        if [[ "$n" == "libc++.1.dylib" ]]; then
          install_name_tool -change "$dep" "/usr/lib/libc++.1.dylib" "$bin" 2>/dev/null || true
          continue
        fi
        case "$dep" in
          /usr/lib/*|/System/*|@rpath/*|@loader_path/*|@executable_path/*) continue ;;
        esac
        if [[ -f "$FW/$n" ]]; then
          install_name_tool -change "$dep" "@rpath/$n" "$bin" 2>/dev/null || true
        fi
      done < <(otool -L "$bin" | awk '/^\t/ {print $1}')
    fi
  done
  shopt -u nullglob
fi

if command -v codesign >/dev/null 2>&1; then
  codesign --force --deep --sign - "$APP" 2>/dev/null || true
fi

cat > "${OUT}/使用说明.txt" <<EOF
PdfToMarkdown ${VERSION}（macOS Apple Silicon）

不要从压缩包窗口里直接双击。

1. 解压后，把 PdfToMarkdown.app 拖到「应用程序」或「桌面」
2. 打开「终端」，执行（二选一）：
   xattr -cr ~/Desktop/PdfToMarkdown.app
   xattr -cr /Applications/PdfToMarkdown.app
3. 按住 Control 点图标 → 打开
4. 选择 PDF；每个 PDF 旁边会生成同名 .md 和 .txt

仅支持 M1/M2/M3/M4，无需联网、无需安装 Python。
Intel Mac 不能用。
EOF

cat > "${OUT}/VERSION.txt" <<EOF
PdfToMarkdown ${VERSION}
Platform: macOS arm64 (Apple Silicon)
Offline. No Homebrew install required.

See 使用说明.txt
EOF

if [[ -z "$BIN" ]]; then
  echo "WARNING: PdfToMarkdown.Cli not found. Skeleton .app created; compile on a Mac with scripts/build_macos.sh" >&2
fi

(cd "$(dirname "$OUT")" && zip -r -q "$(basename "$OUT").zip" "$(basename "$OUT")") || true
echo "Created $APP"
echo "Created ${OUT}.zip"
