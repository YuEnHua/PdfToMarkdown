#!/bin/bash
# PdfToMarkdown.app entry — pick/drop PDFs, then run bundled CLI.
# Output: sibling .md + .txt next to each PDF (same as Windows).
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
CLI="${HERE}/PdfToMarkdown.Cli"
RES="$(cd "${HERE}/../Resources" && pwd)"
MODELS="${RES}/models"
CONFIG="${RES}/config/pdf_to_md.json"

export DYLD_FALLBACK_LIBRARY_PATH="${HERE}:${HERE}/../Frameworks:${DYLD_FALLBACK_LIBRARY_PATH:-}"

notify() {
  local msg="$1"
  osascript -e "display notification \"${msg}\" with title \"PdfToMarkdown\"" >/dev/null 2>&1 || true
}

die() {
  local msg="$1"
  osascript -e "display dialog \"${msg}\" with title \"PdfToMarkdown\" buttons {\"OK\"} default button 1" >/dev/null 2>&1 || echo "$msg" >&2
  exit 1
}

if [[ ! -x "$CLI" ]]; then
  die "未找到 PdfToMarkdown.Cli。请在 Apple Silicon Mac 上运行 scripts/build_macos.sh 后再打开。"
fi

if [[ ! -d "${MODELS}/PP-OCRv6_small_det" ]]; then
  die "缺少模型目录 Resources/models/PP-OCRv6_small_det"
fi

pdfs=()
for arg in "$@"; do
  case "$arg" in
    -psn* | -NSDocumentRevisionsDebugMode) continue ;;
  esac
  lower="$(printf '%s' "$arg" | tr '[:upper:]' '[:lower:]')"
  if [[ "$lower" == *.pdf ]]; then
    pdfs+=("$arg")
  fi
done

if [[ ${#pdfs[@]} -eq 0 ]]; then
  chosen="$(osascript <<'APPLESCRIPT'
set theFiles to choose file with prompt "选择要转换成 Markdown 的 PDF（可多选）" of type {"com.adobe.pdf"} with multiple selections allowed
set out to ""
repeat with f in theFiles
  set out to out & POSIX path of f & linefeed
end repeat
return out
APPLESCRIPT
)" || exit 0
  while IFS= read -r line; do
    [[ -z "$line" ]] && continue
    pdfs+=("$line")
  done <<< "$chosen"
fi

[[ ${#pdfs[@]} -gt 0 ]] || exit 0

notify "开始转换 ${#pdfs[@]} 个 PDF…"

args=()
for p in "${pdfs[@]}"; do
  args+=("$p")
done
args+=(--models "$MODELS")
if [[ -f "$CONFIG" ]]; then
  : # config is baked into Create JSON; models path is enough
fi

if ! "$CLI" "${args[@]}"; then
  die "转换失败。可在终端运行：${CLI} <file.pdf> --models ${MODELS}"
fi

notify "完成：已在各 PDF 旁写入 .md 与 .txt"
exit 0
