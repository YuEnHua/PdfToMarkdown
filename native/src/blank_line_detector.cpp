#include "pdf_to_md/blank_line_detector.h"

#include <algorithm>
#include <cmath>
#include <string>

#include <opencv2/imgproc.hpp>

#include "src/common/geometry_utils.h"

namespace pdf_to_md {
namespace {

struct Seg {
    float x0 = 0;
    float y0 = 0;
    float x1 = 0;
    float y1 = 0;
    float thickness = 0;
};

float MedianOr(std::vector<float> values, float fallback) {
    if (values.empty()) return fallback;
    std::sort(values.begin(), values.end());
    return values[values.size() / 2];
}

float EstimateCharWidth(const std::vector<medical_ocr::OcrTextBox>& boxes,
                        int page_width) {
    std::vector<float> widths;
    for (const auto& b : boxes) {
        if (b.text.empty()) continue;
        float x0, y0, x1, y1;
        medical_ocr::geometry::AxisAlignedBounds(b.points, x0, y0, x1, y1);
        const float w = x1 - x0;
        const float n = static_cast<float>((std::max)(size_t{1}, b.text.size()));
        if (w > 4.0f) widths.push_back(w / n);
    }
    const float fallback =
        (std::max)(8.0f, static_cast<float>(page_width) * 0.012f);
    return MedianOr(std::move(widths), fallback);
}

bool OverlapsTextHeavily(const Seg& seg,
                         const std::vector<medical_ocr::OcrTextBox>& boxes) {
    const float seg_w = (std::max)(1.0f, seg.x1 - seg.x0);
    const float seg_h = (std::max)(1.0f, seg.thickness);
    for (const auto& b : boxes) {
        float x0, y0, x1, y1;
        medical_ocr::geometry::AxisAlignedBounds(b.points, x0, y0, x1, y1);
        const float ix0 = (std::max)(seg.x0, x0);
        const float iy0 = (std::max)(seg.y0 - seg.thickness * 0.5f, y0);
        const float ix1 = (std::min)(seg.x1, x1);
        const float iy1 = (std::min)(seg.y1 + seg.thickness * 0.5f, y1);
        if (ix1 <= ix0 || iy1 <= iy0) continue;
        const float inter = (ix1 - ix0) * (iy1 - iy0);
        const float text_area = (std::max)(1.0f, (x1 - x0) * (y1 - y0));
        // Drop lines that largely sit inside a text glyph box.
        if (inter / text_area > 0.35f) return true;
        // Drop if most of the segment is covered by text horizontally and
        // vertically near the glyph body (not just baseline under blank).
        const float cover = (ix1 - ix0) / seg_w;
        const float text_h = (std::max)(1.0f, y1 - y0);
        const float vert_center = (seg.y0 + seg.y1) * 0.5f;
        const float text_cy = (y0 + y1) * 0.5f;
        if (cover > 0.7f && std::fabs(vert_center - text_cy) < text_h * 0.35f) {
            return true;
        }
        (void)seg_h;
    }
    return false;
}

bool SameLineNear(const Seg& a, const Seg& b, float y_tol, float x_gap) {
    const float ay = (a.y0 + a.y1) * 0.5f;
    const float by = (b.y0 + b.y1) * 0.5f;
    if (std::fabs(ay - by) > y_tol) return false;
    // Gap between segments (allow small break for dashed blanks).
    const float gap = (std::max)(0.0f, (std::max)(a.x0, b.x0) -
                                           (std::min)(a.x1, b.x1));
    // If overlapping in x, gap is 0 via the formula when intervals overlap:
    // max(x0)-min(x1) <= 0.
    const float left_gap =
        (a.x1 < b.x0) ? (b.x0 - a.x1) : ((b.x1 < a.x0) ? (a.x0 - b.x1) : 0.0f);
    return left_gap <= x_gap;
}

std::vector<Seg> MergeCollinear(std::vector<Seg> segs, float y_tol,
                                float x_gap) {
    if (segs.empty()) return segs;
    std::sort(segs.begin(), segs.end(),
              [](const Seg& a, const Seg& b) {
                  const float ay = (a.y0 + a.y1) * 0.5f;
                  const float by = (b.y0 + b.y1) * 0.5f;
                  if (std::fabs(ay - by) > 1.0f) return ay < by;
                  return a.x0 < b.x0;
              });

    std::vector<Seg> merged;
    for (const auto& s : segs) {
        if (!merged.empty() &&
            SameLineNear(merged.back(), s, y_tol, x_gap)) {
            auto& m = merged.back();
            m.x0 = (std::min)(m.x0, s.x0);
            m.x1 = (std::max)(m.x1, s.x1);
            m.y0 = (std::min)(m.y0, s.y0);
            m.y1 = (std::max)(m.y1, s.y1);
            m.thickness = (std::max)(m.thickness, s.thickness);
        } else {
            merged.push_back(s);
        }
    }
    return merged;
}

float EstimateCharHeight(const std::vector<medical_ocr::OcrTextBox>& boxes,
                         float char_w) {
    std::vector<float> heights;
    for (const auto& b : boxes) {
        if (b.text.empty()) continue;
        float x0, y0, x1, y1;
        medical_ocr::geometry::AxisAlignedBounds(b.points, x0, y0, x1, y1);
        const float h = y1 - y0;
        if (h > 4.0f) heights.push_back(h);
    }
    return MedianOr(std::move(heights), (std::max)(12.0f, char_w * 1.8f));
}

medical_ocr::OcrTextBox SegToBox(const Seg& seg, float char_w, float char_h) {
    medical_ocr::OcrTextBox box;
    // Place the blank as a text-sized box whose baseline matches the ink line,
    // so BuildReadingLines clusters it with neighboring words.
    const float baseline = (seg.y0 + seg.y1) * 0.5f;
    const float h = (std::max)(seg.thickness + 2.0f, char_h);
    const float y1 = baseline + seg.thickness * 0.5f + 1.0f;
    const float y0 = y1 - h;
    box.points = {
        medical_ocr::PointF{seg.x0, y0},
        medical_ocr::PointF{seg.x1, y0},
        medical_ocr::PointF{seg.x1, y1},
        medical_ocr::PointF{seg.x0, y1},
    };
    const float width = (std::max)(1.0f, seg.x1 - seg.x0);
    int n = static_cast<int>(std::lround(width / (std::max)(4.0f, char_w)));
    if (n < 3) n = 3;
    if (n > 40) n = 40;
    box.text = std::string(static_cast<size_t>(n), '_');
    box.confidence = 1.0f;
    return box;
}

}  // namespace

std::vector<medical_ocr::OcrTextBox> DetectBlankLines(
    const cv::Mat& page_bgr,
    const std::vector<medical_ocr::OcrTextBox>& existing_boxes,
    const ConvertOptions& options) {
    std::vector<medical_ocr::OcrTextBox> out;
    if (!options.enable_blank_line_detection) return out;
    if (page_bgr.empty()) return out;

    cv::Mat gray;
    if (page_bgr.channels() == 1) {
        gray = page_bgr;
    } else {
        cv::cvtColor(page_bgr, gray, cv::COLOR_BGR2GRAY);
    }

    cv::Mat binary;
    cv::threshold(gray, binary, 0, 255, cv::THRESH_BINARY_INV | cv::THRESH_OTSU);

    const int page_w = binary.cols;
    const int page_h = binary.rows;
    const float char_w = EstimateCharWidth(existing_boxes, page_w);
    const float char_h = EstimateCharHeight(existing_boxes, char_w);

    int min_width = options.blank_min_width_px;
    if (min_width <= 0) {
        min_width = static_cast<int>(std::lround(char_w * 2.0f));
    }
    min_width = (std::max)(8, min_width);

    int max_thick = options.blank_max_thickness_px;
    if (max_thick <= 0) {
        // Scale lightly with DPI (default options assume ~200 DPI).
        max_thick = (std::max)(3, options.dpi / 50);
    }

    // Extract long-ish horizontal runs.
    const int kernel_w = (std::max)(min_width, static_cast<int>(char_w * 1.5f));
    cv::Mat horizontal;
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT,
                                              cv::Size(kernel_w, 1));
    cv::morphologyEx(binary, horizontal, cv::MORPH_OPEN, kernel);

    // Close small gaps in dashed blanks.
    const int close_w = (std::max)(3, static_cast<int>(char_w * 0.8f));
    cv::Mat close_k =
        cv::getStructuringElement(cv::MORPH_RECT, cv::Size(close_w, 1));
    cv::morphologyEx(horizontal, horizontal, cv::MORPH_CLOSE, close_k);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(horizontal, contours, cv::RETR_EXTERNAL,
                     cv::CHAIN_APPROX_SIMPLE);

    const float long_ratio = (options.blank_long_width_ratio > 0.05f)
                                 ? options.blank_long_width_ratio
                                 : 0.45f;
    const float long_min = static_cast<float>(page_w) * long_ratio;
    const float margin_y = static_cast<float>(page_h) * 0.02f;

    std::vector<Seg> segs;
    segs.reserve(contours.size());
    for (const auto& c : contours) {
        cv::Rect r = cv::boundingRect(c);
        if (r.width < min_width) continue;
        if (r.height > max_thick) continue;
        if (r.height < 1) continue;
        // Aspect: must look like a line.
        if (static_cast<float>(r.width) < static_cast<float>(r.height) * 4.0f) {
            continue;
        }
        // Skip extreme page edges (often scan borders).
        if (r.y < margin_y || r.y + r.height > page_h - margin_y) continue;

        Seg s;
        s.x0 = static_cast<float>(r.x);
        s.x1 = static_cast<float>(r.x + r.width);
        s.y0 = static_cast<float>(r.y);
        s.y1 = static_cast<float>(r.y + r.height);
        s.thickness = static_cast<float>(r.height);

        // Accept short blanks (>= min_width) and long writing lines.
        // Both already passed min_width; long_min is informational for
        // underscore length only.
        (void)long_min;
        segs.push_back(s);
    }

    const float y_tol = (std::max)(3.0f, static_cast<float>(max_thick) * 1.5f);
    // Only bridge tiny dashed-blank gaps, not separate fill-ins.
    const float x_gap = (std::max)(3.0f, char_w * 0.35f);
    segs = MergeCollinear(std::move(segs), y_tol, x_gap);

    out.reserve(segs.size());
    for (const auto& s : segs) {
        if (OverlapsTextHeavily(s, existing_boxes)) continue;
        out.push_back(SegToBox(s, char_w, char_h));
    }
    return out;
}

}  // namespace pdf_to_md
