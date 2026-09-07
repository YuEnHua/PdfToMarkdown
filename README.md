# PdfToMarkdown

离线 PDF OCR → Markdown（Windows 11 x64 / macOS Apple Silicon）。

独立项目路径：`D:\work\AAA_21ic_Project\PdfToMarkdown`  
与 `MedicalOCR` 同级，通过相对路径复用其 PP-OCRv6 / Paddle OCR 源码与 cmake 模块。

## 功能（v1）

- PDFium 分页渲染（内存中完成，不落盘临时页图）
- PP-OCRv6 Small（det + rec）识别
- 基础 Markdown：正文、段落、分页标记 `<!-- page: N -->`
- Win11 桌面界面 + CLI；macOS 为 `.app` + CLI
- 完全离线；日志不包含 OCR 正文

## 依赖

### Windows 11 x64
- VS 2022 Build Tools（MSVC）、CMake 3.16+
- 同级目录：`../MedicalOCR`
- OpenCV 4.7：`D:/Environment/opencv/build`
- Paddle Inference 3.3.0：`D:/Environment/paddle_inference_3.3.0`
- 模型：`D:/Environment/PaddleOCR-models/PP-OCRv6`

### macOS Apple Silicon (arm64)
必须在 **Mac 上编译一次**（Windows 交叉编译无法生成可运行的 Mach-O）：
- Xcode Command Line Tools、CMake 3.16+、Python 3
- 同级 `../MedicalOCR`
- 预编译库由脚本从网页下载（不装 Homebrew）：`python scripts/fetch_macos_deps.py`
  - PDFium `chromium/7961` mac-arm64
  - Paddle Inference macOS m1（3.2.1 / 回退 3.0.0）
  - conda-forge `libopencv` osx-arm64
- 模型与 Windows 相同（PP-OCRv6 small det/rec）

## 构建

Windows:

```powershell
cd D:\work\AAA_21ic_Project\PdfToMarkdown
.\scripts\build_all.ps1
```

或手动：

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 `
  -DPADDLE_INFERENCE_DIR=D:/Environment/paddle_inference_3.3.0 `
  -DOpenCV_DIR=D:/Environment/opencv/build `
  -DMEDICAL_OCR_ROOT=D:/work/AAA_21ic_Project/MedicalOCR
cmake --build build --config Release --target PdfToMarkdown.Gui PdfToMarkdown.Cli
```

macOS（Apple Silicon）：

```bash
chmod +x scripts/*.sh app/macos/launcher.sh
./scripts/build_macos.sh
```

## 运行

```text
build\bin\Release\PdfToMarkdown.Gui.exe
PdfToMarkdown.Cli.exe scan.pdf --models D:\Environment\PaddleOCR-models\PP-OCRv6 --dpi 200
PdfToMarkdown.Cli.exe a.pdf b.pdf --models D:\Environment\PaddleOCR-models\PP-OCRv6
```

每次转换同时写同名 `.md` 与 `.txt`；默认每页落盘。

## 发布包

```powershell
.\scripts\package_release.ps1
```

产物：`release/PdfToMarkdown-1.0.0-windows-x64.zip`

macOS（需在 Apple Silicon 上 `./scripts/build_macos.sh`）：

```powershell
.\scripts\fetch_macos_deps.ps1
.\scripts\package_macos.ps1    # 仅骨架：dylib + 启动器 + 模型，不含 Cli
```

```bash
./scripts/build_macos.sh       # 在 Mac 上：编译 + 免安装 .app
```

产物：`release/PdfToMarkdown-1.0.0-macos-arm64.zip`

没有 Mac 时可用 **GitHub Actions**（`macos-14` Apple Silicon）云端编译。

### GitHub Actions（云端编 Mac 包）

本仓库没有配置 GitHub 远程时，需要你在网页上建两个仓库并推送：

1. GitHub 新建公开仓库 `PdfToMarkdown`（私有仓库免费额度通常 **不能** 跑 macOS runner）
2. 同一账号再建公开仓库 `MedicalOCR`（workflow 会按 `你的用户名/MedicalOCR` 拉取同级源码）
3. 把本目录推到 `PdfToMarkdown`，把 `../MedicalOCR` 推到 `MedicalOCR`（不要推 `build/`、`third_party/paddle*` 大目录）
4. 打开 `PdfToMarkdown` 仓库 → **Actions** → 允许 workflows → 选 **macos-arm64** → **Run workflow**
5. 跑完（约 20–40 分钟）→ 打开该次 run → **Artifacts** → 下载 `PdfToMarkdown-1.0.0-macos-arm64`

把 zip 解压确认存在 `PdfToMarkdown.app/Contents/MacOS/PdfToMarkdown.Cli` 后再发给用户。

## 首版范围外

表格结构、标题层级、图片提取、公式、多栏高保真、PDF 文本层直读。
