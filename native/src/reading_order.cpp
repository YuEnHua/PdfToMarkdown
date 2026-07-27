#include "pdf_to_md/reading_order.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <sstream>

#include "src/common/geometry_utils.h"

namespace pdf_to_md {
namespace {

float BoxHeight(const medical_ocr::BBox& box) {
    float x0, y0, x1, y1;
    medical_ocr::geometry::AxisAlignedBounds(box, x0, y0, x1, y1);
    return (std::max)(1.0f, y1 - y0);
}

float MedianPositive(std::vector<float> values) {
    if (values.empty()) return 16.0f;
    std::sort(values.begin(), values.end());
    return values[values.size() / 2];
}

std::string JoinBoxes(const std::vector<medical_ocr::OcrTextBox>& boxes) {
    std::ostringstream oss;
    bool first = true;
    for (const auto& b : boxes) {
        if (b.text.empty()) continue;
        if (!first) oss << ' ';
        oss << b.text;
        first = false;
    }
    return oss.str();
}

}  // namespace

std::vector<TextLine> BuildReadingLines(const medical_ocr::OcrResult& ocr,
                                        float line_y_tolerance_ratio) {
    std::vector<TextLine> lines;
    if (ocr.boxes.empty()) return lines;

    std::vector<medical_ocr::OcrTextBox> boxes = ocr.boxes;
    std::sort(boxes.begin(), boxes.end(),
              [](const medical_ocr::OcrTextBox& a,
                 const medical_ocr::OcrTextBox& b) {
                  auto ca = medical_ocr::geometry::BoxCenter(a.points);
                  auto cb = medical_ocr::geometry::BoxCenter(b.points);
                  if (std::fabs(ca.y - cb.y) > 1.0f) return ca.y < cb.y;
                  return ca.x < cb.x;
              });

    std::vector<float> heights;
    heights.reserve(boxes.size());
    for (const auto& b : boxes) {
        heights.push_back(BoxHeight(b.points));
    }
    const float median_h = MedianPositive(heights);
    const float y_tol =
        (std::max)(4.0f, median_h * (std::max)(0.1f, line_y_tolerance_ratio));

    for (const auto& box : boxes) {
        if (box.text.empty()) continue;
        const auto center = medical_ocr::geometry::BoxCenter(box.points);
        const float h = BoxHeight(box.points);

        TextLine* best = nullptr;
        float best_dy = y_tol + 1.0f;
        for (auto& line : lines) {
            const float dy = std::fabs(line.y_center - center.y);
            if (dy <= y_tol && dy < best_dy) {
                best = &line;
                best_dy = dy;
            }
        }

        if (!best) {
            TextLine line;
            line.y_center = center.y;
            line.height = h;
            line.boxes.push_back(box);
            lines.push_back(std::move(line));
        } else {
            const float n = static_cast<float>(best->boxes.size());
            best->y_center = (best->y_center * n + center.y) / (n + 1.0f);
            best->height = (best->height * n + h) / (n + 1.0f);
            best->boxes.push_back(box);
        }
    }

    std::sort(lines.begin(), lines.end(),
              [](const TextLine& a, const TextLine& b) {
                  return a.y_center < b.y_center;
              });

    for (auto& line : lines) {
        std::sort(line.boxes.begin(), line.boxes.end(),
                  [](const medical_ocr::OcrTextBox& a,
                     const medical_ocr::OcrTextBox& b) {
                      return medical_ocr::geometry::BoxCenter(a.points).x <
                             medical_ocr::geometry::BoxCenter(b.points).x;
                  });
        line.text = JoinBoxes(line.boxes);
    }

    return lines;
}

std::vector<std::string> MergeParagraphs(const std::vector<TextLine>& lines,
                                         float paragraph_gap_ratio) {
    std::vector<std::string> paragraphs;
    if (lines.empty()) return paragraphs;

    std::vector<float> heights;
    for (const auto& line : lines) {
        heights.push_back((std::max)(1.0f, line.height));
    }
    const float median_h = MedianPositive(heights);
    const float gap_thresh =
        (std::max)(8.0f, median_h * (std::max)(0.5f, paragraph_gap_ratio));

    std::string current;
    float prev_y = lines.front().y_center;
    float prev_h = lines.front().height;

    for (size_t i = 0; i < lines.size(); ++i) {
        const auto& line = lines[i];
        if (line.text.empty()) continue;

        if (i == 0) {
            current = line.text;
            prev_y = line.y_center;
            prev_h = line.height;
            continue;
        }

        const float gap = line.y_center - prev_y;
        const float expected = (prev_h + line.height) * 0.5f;
        if (gap > expected + gap_thresh) {
            if (!current.empty()) paragraphs.push_back(current);
            current = line.text;
        } else {
            if (!current.empty()) current.push_back('\n');
            current += line.text;
        }
        prev_y = line.y_center;
        prev_h = line.height;
    }

    if (!current.empty()) paragraphs.push_back(current);
    return paragraphs;
}

PageMarkdown PageToMarkdown(int page_index,
                            const medical_ocr::OcrResult& ocr,
                            const ConvertOptions& options) {
    PageMarkdown page;
    page.page_index = page_index;
    auto lines =
        BuildReadingLines(ocr, options.line_y_tolerance_ratio);
    page.paragraphs = MergeParagraphs(lines, options.paragraph_gap_ratio);
    return page;
}

}  // namespace pdf_to_md
