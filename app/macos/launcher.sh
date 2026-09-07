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
  printf '%s\n' "$msg" >&2
  local tmp
  tmp="$(mktemp /tmp/pdfmd-msg.XXXXXX)" || tmp="/tmp/pdfmd-msg.$$"
  printf '%s' "$msg" > "$tmp"
  osascript -e "set msg to (do shell script \"cat \" & quoted form of \"$tmp\")" \
    -e 'display dialog msg with title "PdfToMarkdown" buttons {"OK"} default button 1' \
    >/dev/null 2>&1 || true
  rm -f "$tmp"
  exit 1
}

if [[ "$HERE" == *"/AppTranslocation/"* ]]; then
  die "请勿从压缩包或下载文件夹里直接打开。请先把 PdfToMarkdown.app 拖到「应用程序」或「桌面」，打开「终端」复制执行：

xattr -cr ~/Desktop/PdfToMarkdown.app

（若放在应用程序里则改为：xattr -cr /Applications/PdfToMarkdown.app）

然后按住 Control 点图标 → 打开。"
fi

if [[ ! -x "$CLI" ]]; then
  die "未找到 PdfToMarkdown.Cli。请确认解压后的 PdfToMarkdown.app 完整。"
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

set +e
cli_out="$("$CLI" "${args[@]}" 2>&1)"
cli_status=$?
set -e
if [[ $cli_status -ne 0 ]]; then
  tail_out="$(printf '%s' "$cli_out" | tail -c 600)"
  die "转换失败。请把 PdfToMarkdown.app 放到「应用程序」或「桌面」后执行：xattr -cr <app路径>，再右键打开。

${tail_out}"
fi

notify "完成：已在各 PDF 旁写入 .md 与 .txt"
exit 0
