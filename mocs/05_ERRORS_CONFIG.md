# 05 — 错误码、配置、取消

## 错误码（`error_codes.h`）

| 码 | 宏 | 常见原因 |
|----|-----|----------|
| 0 | `PDFMD_OK` | 成功 |
| 1 | `PDFMD_ERR_INVALID_ARG` | 空路径、空 models、空 handle |
| 2 | `PDFMD_ERR_NOT_INITIALIZED` | 未 Create 就 Convert |
| 3 | `PDFMD_ERR_ALREADY_INITIALIZED` | （预留/内部） |
| 4 | `PDFMD_ERR_PDF_OPEN` | 文件不存在、损坏、读失败 |
| 5 | `PDFMD_ERR_PDF_ENCRYPTED` | 需要密码 |
| 6 | `PDFMD_ERR_PDF_EMPTY` | 0 页 |
| 7 | `PDFMD_ERR_PDF_RENDER` | 某一页渲染失败 |
| 8 | `PDFMD_ERR_OCR_INIT` | 引擎初始化失败 |
| 9 | `PDFMD_ERR_OCR_FAILED` | 某一页 RecognizeMat 失败 |
| 10 | `PDFMD_ERR_MODEL_MISSING` | det/rec 目录缺 inference 文件 |
| 11 | `PDFMD_ERR_WRITE_FAILED` | 无法写 MD / 磁盘满 / 权限 |
| 12 | `PDFMD_ERR_CANCELLED` | 用户取消 |
| 13 | `PDFMD_ERR_BUSY` | 同一 handle 上并发 Convert |
| 99 | `PDFMD_ERR_INTERNAL` | 未预期异常 |

取文案：`PdfToMd_GetLastError(handle)`（UTF-8）。

---

## 配置 JSON

Create 时传入，或参考 `config/pdf_to_md.json`：

```json
{
  "dpi": 200,
  "cpu_threads": 8,
  "minimum_confidence": 0.0,
  "enable_mkldnn": false,
  "line_y_tolerance_ratio": 0.6,
  "paragraph_gap_ratio": 1.8,
  "enable_blank_line_detection": true,
  "enable_column_detection": true,
  "column_gap_min_ratio": 0.10,
  "column_min_boxes_per_side": 2
}
```

| 字段 | 流向 |
|------|------|
| `dpi` | `ConvertOptions` → `RenderPage` |
| `cpu_threads` / `minimum_confidence` / `enable_mkldnn` | 传给 `PaddleOcrEngine::Initialize` |
| `line_y_tolerance_ratio` / `paragraph_gap_ratio` | `PageToMarkdown` |
| `enable_*_blank_*` | `blank_line_detector` |
| `enable_column_detection` / `column_*` | `FindColumnSplitX` / `PageToMarkdown` |

DPI 在代码里会钳到 **150–300**。

---

## 取消语义

```text
Cancel() 只置位 cancel_requested_
真正停止点：页循环顶部 / 渲染后 / 全部页后写文件前
```

因此取消可能发生在「当前页 OCR 已开始之后」，最坏再等一页 OCR 结束。

---

## 进度回调约定

- `message_utf8`：**不要**假设含识别正文
- `current_page`：完成页数语义（Finished 时为 i+1）；开始阶段可为 0
- GUI 用 `PostMessage` 切回 UI 线程；回调里不要直接碰 HWND 控件（当前实现已遵守）

---

## 快速自测清单

1. `PdfToMarkdown.Tests.exe` — 纯逻辑  
2. CLI 跑 `samples/sample_a4_scan.pdf` — 端到端 OCR  
3. GUI 拖入同一 PDF — 进度条与预览  
4. 转换中点取消 — 应得到取消状态而非崩溃  
5. 错误模型路径 — Create 失败，错误信息可读  

```powershell
cd D:\work\AAA_21ic_Project\PdfToMarkdown
.\build\bin\Release\PdfToMarkdown.Tests.exe
.\scripts\run_integration.ps1
.\build\bin\Release\PdfToMarkdown.Gui.exe
```
