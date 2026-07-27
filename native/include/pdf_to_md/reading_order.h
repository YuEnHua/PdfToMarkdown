#ifndef PDF_TO_MD_READING_ORDER_H_
#define PDF_TO_MD_READING_ORDER_H_

#include <string>
#include <utility>
#include <vector>

#include "medical_ocr/types.h"
#include "pdf_to_md/types.h"

namespace pdf_to_md {

/**
 * Cluster OCR boxes into reading-order lines (Y then X), join with spaces.
 */
std::vector<TextLine> BuildReadingLines(
    const medical_ocr::OcrResult& ocr,
    float line_y_tolerance_ratio = 0.6f);

std::vector<TextLine> BuildReadingLinesFromBoxes(
    const std::vector<medical_ocr::OcrTextBox>& boxes,
    float line_y_tolerance_ratio = 0.6f);

std::vector<std::string> MergeParagraphs(
    const std::vector<TextLine>& lines,
    float paragraph_gap_ratio = 1.8f);

/**
 * Detect a left|right column split using a large vertical gap in X coverage.
 * Returns true and sets split_x when both sides have enough text boxes.
 * Image-above-text / single-column pages typically return false.
 */
bool FindColumnSplitX(
    const std::vector<medical_ocr::OcrTextBox>& boxes,
    int page_width,
    const ConvertOptions& options,
    float& split_x_out);

/**
 * Build page markdown. With column detection enabled, reads left column
 * fully then right column (gap is a boundary, not filled with spaces).
 */
PageMarkdown PageToMarkdown(
    int page_index,
    const medical_ocr::OcrResult& ocr,
    const ConvertOptions& options);

}  // namespace pdf_to_md

#endif  // PDF_TO_MD_READING_ORDER_H_
