#include "pdf_to_md/reading_order.h"

#include <algorithm>
#include <cmath>
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

int EstimatePageWidth(const std::vector<medical_ocr::OcrTextBox>& boxes,
                      int page_width) {
    if (page_width > 0) return page_width;
    float max_x = 0.0f;
    for (const auto& b : boxes) {
        float x0, y0, x1, y1;
        medical_ocr::geometry::AxisAlignedBounds(b.points, x0, y0, x1, y1);
        max_x = (std::max)(max_x, x1);
    }
    return static_cast<int>(std::ceil(max_x));
}

}  // namespace

std::vector<TextLine> BuildReadingLinesFromBoxes(
    const std::vector<medical_ocr::OcrTextBox>& input_boxes,
    float line_y_tolerance_ratio) {
    std::vector<TextLine> lines;
    if (input_boxes.empty()) return lines;

    std::vector<medical_ocr::OcrTextBox> boxes = input_boxes;
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

std::vector<TextLine> BuildReadingLines(const medical_ocr::OcrResult& ocr,
                                        float line_y_tolerance_ratio) {
    return BuildReadingLinesFromBoxes(ocr.boxes, line_y_tolerance_ratio);
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

bool FindColumnSplitX(const std::vector<medical_ocr::OcrTextBox>& boxes,
                      int page_width,
                      const ConvertOptions& options,
                      float& split_x_out) {
    split_x_out = 0.0f;
    if (!options.enable_column_detection) return false;

    std::vector<medical_ocr::OcrTextBox> usable;
    usable.reserve(boxes.size());
    for (const auto& b : boxes) {
        if (!b.text.empty()) usable.push_back(b);
    }
    const int min_boxes = (std::max)(1, options.column_min_boxes_per_side);
    if (static_cast<int>(usable.size()) < min_boxes * 2) return false;

    const int width = EstimatePageWidth(usable, page_width);
    if (width < 32) return false;

    struct Interval {
        float x0;
        float x1;
    };
    std::vector<Interval> intervals;
    intervals.reserve(usable.size());
    for (const auto& b : usable) {
        float x0, y0, x1, y1;
        medical_ocr::geometry::AxisAlignedBounds(b.points, x0, y0, x1, y1);
        if (x1 > x0) intervals.push_back({x0, x1});
    }
    if (intervals.size() < 2) return false;

    std::sort(intervals.begin(), intervals.end(),
              [](const Interval& a, const Interval& b) { return a.x0 < b.x0; });

    // Sweep: track covered right edge, find empty horizontal gaps.
    float cover_right = intervals.front().x1;
    float best_gap = 0.0f;
    float best_mid = 0.0f;

    const float min_gap =
        static_cast<float>(width) *
        (std::max)(0.05f, options.column_gap_min_ratio);
    const float band_lo = static_cast<float>(width) * 0.20f;
    const float band_hi = static_cast<float>(width) * 0.80f;

    for (size_t i = 1; i < intervals.size(); ++i) {
        const float next_left = intervals[i].x0;
        if (next_left > cover_right) {
            const float gap = next_left - cover_right;
            const float mid = (cover_right + next_left) * 0.5f;
            if (gap >= min_gap && mid >= band_lo && mid <= band_hi &&
                gap > best_gap) {
                best_gap = gap;
                best_mid = mid;
            }
        }
        cover_right = (std::max)(cover_right, intervals[i].x1);
    }

    if (best_gap < min_gap) return false;

    int left_n = 0;
    int right_n = 0;
    for (const auto& b : usable) {
        const auto c = medical_ocr::geometry::BoxCenter(b.points);
        if (c.x < best_mid) {
            ++left_n;
        } else {
            ++right_n;
        }
    }
    if (left_n < min_boxes || right_n < min_boxes) return false;

    split_x_out = best_mid;
    return true;
}

PageMarkdown PageToMarkdown(int page_index,
                            const medical_ocr::OcrResult& ocr,
                            const ConvertOptions& options) {
    PageMarkdown page;
    page.page_index = page_index;

    float split_x = 0.0f;
    const bool split = FindColumnSplitX(ocr.boxes, ocr.imageWidth, options,
                                        split_x);

    if (!split) {
        auto lines =
            BuildReadingLines(ocr, options.line_y_tolerance_ratio);
        page.paragraphs =
            MergeParagraphs(lines, options.paragraph_gap_ratio);
        return page;
    }

    std::vector<medical_ocr::OcrTextBox> left;
    std::vector<medical_ocr::OcrTextBox> right;
    left.reserve(ocr.boxes.size());
    right.reserve(ocr.boxes.size());
    for (const auto& b : ocr.boxes) {
        if (b.text.empty()) continue;
        const auto c = medical_ocr::geometry::BoxCenter(b.points);
        if (c.x < split_x) {
            left.push_back(b);
        } else {
            right.push_back(b);
        }
    }

    auto left_lines =
        BuildReadingLinesFromBoxes(left, options.line_y_tolerance_ratio);
    auto right_lines =
        BuildReadingLinesFromBoxes(right, options.line_y_tolerance_ratio);

    auto left_paras =
        MergeParagraphs(left_lines, options.paragraph_gap_ratio);
    auto right_paras =
        MergeParagraphs(right_lines, options.paragraph_gap_ratio);

    page.paragraphs = std::move(left_paras);
    // Column break: start right column as new paragraphs (no space-fill).
    page.paragraphs.insert(page.paragraphs.end(), right_paras.begin(),
                           right_paras.end());
    return page;
}

}  // namespace pdf_to_md
