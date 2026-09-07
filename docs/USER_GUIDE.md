# PdfToMarkdown 使用说明

## 安装 / 便携运行

1. 解压 `PdfToMarkdown-*-windows-x64.zip`
2. 确认 `models/PP-OCRv6_small_det` 与 `models/PP-OCRv6_small_rec` 存在
3. 双击 `bin/PdfToMarkdown.Gui.exe`

无需安装 Python，无需联网。

## macOS（Apple Silicon）

1. 在 Mac 上执行 `./scripts/build_macos.sh`（会下载预编译库并生成 `.app`）
2. 解压 `release/PdfToMarkdown-*-macos-arm64.zip`
3. 双击 `PdfToMarkdown.app`（首次若被拦截：右键 → 打开）
4. 选择一个或多个 PDF；结果写在各 PDF 同目录的 `.md` 与 `.txt`

也可把 PDF 拖到应用图标上。命令行：

```bash
PdfToMarkdown.app/Contents/MacOS/PdfToMarkdown.Cli a.pdf b.pdf \
  --models PdfToMarkdown.app/Contents/Resources/models
```

**不能在 Windows 上交叉编译出可运行的 Mac 程序。** 在 Windows 跑 `.\scripts\package_macos.ps1` 只会打出带 dylib/模型的骨架，缺 `PdfToMarkdown.Cli`。

## 界面操作

1. **浏览可多选**，或一次拖入多个 PDF（列表框每行一个路径）
2. 输出：每个 PDF 旁同名写入 `.md` 与 `.txt`（单文件时可改 `.md` 路径；`.txt` 始终跟 md 同基名）
3. 确认模型目录（默认已指向发布包内 `models`）
4. DPI 建议 200（清晰扫描可用 150，小字可用 300）
5. 点击「开始转换」，可随时「取消」（会停后续文件；已完成页已落盘）
6. 完成后可预览并打开 Markdown / 所在目录

每页 OCR 完成后会原子重写输出文件，避免程序中途退出丢失进度。

## 命令行

单文件（可指定 `-o`；同时写同基名 `.txt`）：

```bat
PdfToMarkdown.Cli.exe input.pdf -o out.md --models ..\models --dpi 200 --threads 8
```

省略 `-o` 时默认同目录同名 `.md`：

```bat
PdfToMarkdown.Cli.exe input.pdf --models ..\models
```

批量（省略 `-o`，每个 PDF 旁写 `.md`/`.txt`；不可对批量使用单个 `-o`）：

```bat
PdfToMarkdown.Cli.exe a.pdf b.pdf c.pdf --models ..\models --dpi 200
```

退出码：`0` 全部成功；非 0 见 `error_codes.h`（如加密 PDF、取消、写盘失败）；批量部分失败也可能返回非 0。

## 性能建议

- 默认串行按页 OCR，批量也是串行多文件，避免 CPU/MKL 过度并行
- 百页文档优先 150–200 DPI
- 关闭其他占满 CPU 的任务

## 故障排查

| 现象 | 处理 |
|------|------|
| 初始化失败 / model missing | 检查 models 目录是否含 det/rec 的 inference.json/pdiparams/yml |
| PDF 加密 | 先解密再转换 |
| 缺少 DLL | 使用官方发布包，勿只拷贝 exe |
| 识别乱序 | 已支持左右分栏（方案 A）；更复杂版式仍可能不准 |
| 中文路径失败 | 使用 GUI 或确保以 UTF-8 传参 |

## 隐私

日志仅记录页号、耗时、错误码，**不写入 OCR 正文**。本地离线处理。
