# 00 — 项目总览

## 一句话

**把扫描版/图片型 PDF 离线 OCR 成基础 Markdown**（正文 + 分段 + 分页标记），提供 Win11 GUI 和 CLI。

## 不做的事（v1）

- 不识别表格结构 / 标题层级 / 公式 / 多栏版式
- 不抽取 PDF 内嵌图片
- 不走云端、不依赖 Python 运行时
- 不把 OCR 正文写进日志（隐私）

## 和 MedicalOCR 的关系

```text
D:\work\AAA_21ic_Project\
├── MedicalOCR\          # 医院报告结构化 OCR 库
│   ├── src\ocr\paddle_ocr_engine.*   ← PdfToMarkdown 复用
│   ├── src\common\geometry_utils.*   ← 阅读顺序用
│   ├── include\medical_ocr\types.h   ← OcrResult / OcrTextBox
│   └── cmake\PaddleOcrVendor.cmake   ← Paddle 链接
└── PdfToMarkdown\       # 本项目（独立 CMake）
    └── MEDICAL_OCR_ROOT = ../MedicalOCR
```

PdfToMarkdown **不是** MedicalOCR 子目录了；构建时通过 `MEDICAL_OCR_ROOT` 引用同级工程源码。

## 产物一览

| 文件 | 角色 |
|------|------|
| `PdfToMarkdown.Native.dll` | 核心：PDF 渲染、OCR、排版、写 MD |
| `PdfToMarkdown.Gui.exe` | Win32 桌面界面（拖放、进度、取消、预览） |
| `PdfToMarkdown.Cli.exe` | 命令行批处理 |
| `PdfToMarkdown.Tests.exe` | 单元测试（阅读顺序、Markdown 等） |

## 源码目录地图

```text
PdfToMarkdown/
├── native/
│   ├── include/pdf_to_md/     # 公开头文件（C API + 内部模块声明）
│   └── src/                   # 实现
├── cli/main.cpp               # CLI 入口
├── app/win32_gui/main.cpp     # GUI 入口
├── app/PdfToMarkdown.App/     # 可选 WinUI 3（需完整 VS）
├── config/pdf_to_md.json      # 默认配置样例
├── tests/                     # GoogleTest
├── scripts/                   # 构建 / 打包 / 集成测试
├── third_party/pdfium/        # PDF 渲染库
├── samples/                   # 样例 PDF / 黄金 MD
├── docs/                      # 用户指南、基准
└── mocs/                      # 你正在看的学习笔记
```

## 运行时依赖（DLL）

与 `Native.dll` 同目录通常需要：

- `pdfium.dll` — PDF 渲染
- `paddle_inference.dll` / `common.dll` / `phi.dll` — 推理
- `mklml.dll` / `libiomp5md.dll` — CPU 数学库
- `opencv_world470.dll` — 图像
- `polyclipping.dll` — OCR 裁剪多边形

模型目录需含：

```text
models/
├── PP-OCRv6_small_det/   # inference.json + .pdiparams + .yml
└── PP-OCRv6_small_rec/
```
