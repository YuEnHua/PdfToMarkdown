#ifndef PDF_TO_MD_TYPES_H_
#define PDF_TO_MD_TYPES_H_

#include <atomic>
#include <functional>
#include <string>
#include <vector>

#include "medical_ocr/types.h"

namespace pdf_to_md {

struct ConvertOptions {
    int dpi = 200;
    int cpu_threads = 8;
    float minimum_confidence = 0.0f;
    float line_y_tolerance_ratio = 0.6f;   // relative to median box height
    float paragraph_gap_ratio = 1.8f;      // relative to median line height

    /// Detect printed fill-in blank lines and map them to "____" text boxes.
    bool enable_blank_line_detection = true;
    /// Minimum blank segment width in pixels (0 = auto from char width).
    int blank_min_width_px = 0;
    /// Maximum blank line thickness in pixels (0 = auto from dpi).
    int blank_max_thickness_px = 0;
    /// Relative page width for treating a blank as a long writing line.
    float blank_long_width_ratio = 0.45f;
};

struct TextLine {
    float y_center = 0.0f;
    float height = 0.0f;
    std::vector<medical_ocr::OcrTextBox> boxes;
    std::string text;
};

struct PageMarkdown {
    int page_index = 0;  // 0-based
    std::vector<std::string> paragraphs;
};

using ProgressCallback =
    std::function<void(int current_page, int total_pages, const std::string& message)>;

}  // namespace pdf_to_md

#endif  // PDF_TO_MD_TYPES_H_
