# PdfToMarkdown

离线 PDF OCR → Markdown（Windows 11 x64）。

独立项目路径：`D:\work\AAA_21ic_Project\PdfToMarkdown`  
与 `MedicalOCR` 同级，通过相对路径复用其 PP-OCRv6 / Paddle OCR 源码与 cmake 模块。

## 功能（v1）

- PDFium 分页渲染（内存中完成，不落盘临时页图）
- PP-OCRv6 Small（det + rec）识别
- 基础 Markdown：正文、段落、分页标记 `<!-- page: N -->`
- Win11 桌面界面 + CLI + 可选 WinUI 3 工程
- 完全离线；日志不包含 OCR 正文

## 依赖

- Windows 11 x64
- VS 2022 Build Tools（MSVC）、CMake 3.16+
- 同级目录：`../MedicalOCR`
- OpenCV 4.7：`D:/Environment/opencv/build`
- Paddle Inference 3.3.0：`D:/Environment/paddle_inference_3.3.0`
- 模型：`D:/Environment/PaddleOCR-models/PP-OCRv6`

## 构建

```powershell
cd D:\work\AAA_21ic_Project\PdfToMarkdown
.\scripts\build_all.ps1
```

或：

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 `
  -DPADDLE_INFERENCE_DIR=D:/Environment/paddle_inference_3.3.0 `
  -DOpenCV_DIR=D:/Environment/opencv/build `
  -DMEDICAL_OCR_ROOT=D:/work/AAA_21ic_Project/MedicalOCR
cmake --build build --config Release --target PdfToMarkdown.Gui PdfToMarkdown.Cli
```

## 运行

```text
build\bin\Release\PdfToMarkdown.Gui.exe
PdfToMarkdown.Cli.exe scan.pdf -o scan.md --models D:\Environment\PaddleOCR-models\PP-OCRv6 --dpi 200
```

## 发布包

```powershell
.\scripts\package_release.ps1
```

产物：`release/PdfToMarkdown-1.0.0-windows-x64.zip`

## 首版范围外

表格结构、标题层级、图片提取、公式、多栏高保真、PDF 文本层直读。
