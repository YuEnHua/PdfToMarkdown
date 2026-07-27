# PdfToMarkdown 学习笔记（mocs）

本目录帮助理解项目：**做什么、怎么分层、数据怎么流、函数怎么调**。

| 文档 | 内容 |
|------|------|
| [00_OVERVIEW.md](00_OVERVIEW.md) | 一句话目标、目录地图、和 MedicalOCR 的关系 |
| [01_ARCHITECTURE.md](01_ARCHITECTURE.md) | 三层架构、进程/线程、依赖 |
| [02_DATA_FLOW.md](02_DATA_FLOW.md) | 数据流：PDF → 页图 → OCR → 行/段 → Markdown |
| [03_CALL_CHAINS.md](03_CALL_CHAINS.md) | 函数调用链（GUI / CLI / Native） |
| [04_MODULES.md](04_MODULES.md) | 各模块职责与关键类型 |
| [05_ERRORS_CONFIG.md](05_ERRORS_CONFIG.md) | 错误码、配置项、取消与进度 |

填空横线（句中短空 / 翻译长线）见 [02_DATA_FLOW.md](02_DATA_FLOW.md) 末节，实现为 `blank_line_detector`。

建议阅读顺序：`00 → 01 → 02 → 03 → 04`。
