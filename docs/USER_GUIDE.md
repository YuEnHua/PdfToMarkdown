# PdfToMarkdown 使用说明

## 安装 / 便携运行

1. 解压 `PdfToMarkdown-*-windows-x64.zip`
2. 确认 `models/PP-OCRv6_small_det` 与 `models/PP-OCRv6_small_rec` 存在
3. 双击 `bin/PdfToMarkdown.Gui.exe`

无需安装 Python，无需联网。

## 界面操作

1. 选择或拖入 PDF
2. 确认输出 `.md` 路径
3. 确认模型目录（默认已指向发布包内 `models`）
4. DPI 建议 200（清晰扫描可用 150，小字可用 300）
5. 点击「开始转换」，可随时「取消」
6. 完成后可预览并打开 Markdown / 所在目录

## 命令行

```bat
PdfToMarkdown.Cli.exe input.pdf -o out.md --models ..\models --dpi 200 --threads 8
```

退出码：`0` 成功；非 0 见 `error_codes.h`（如加密 PDF、取消、写盘失败）。

## 性能建议

- 默认串行按页 OCR，避免 CPU/MKL 过度并行导致抖动
- 百页文档优先 150–200 DPI
- 关闭其他占满 CPU 的任务

## 故障排查

| 现象 | 处理 |
|------|------|
| 初始化失败 / model missing | 检查 models 目录是否含 det/rec 的 inference.json/pdiparams/yml |
| PDF 加密 | 先解密再转换 |
| 缺少 DLL | 使用官方发布包，勿只拷贝 exe |
| 识别乱序 | v1 仅单栏阅读顺序；复杂多栏属后续版本 |
| 中文路径失败 | 使用 GUI 或确保以 UTF-8 传参 |

## 隐私

日志仅记录页号、耗时、错误码，**不写入 OCR 正文**。本地离线处理。
