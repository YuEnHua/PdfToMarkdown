#ifndef PDF_TO_MD_BLANK_LINE_DETECTOR_H_
#define PDF_TO_MD_BLANK_LINE_DETECTOR_H_

#include <vector>

#include <opencv2/core.hpp>

#include "medical_ocr/types.h"
#include "pdf_to_md/types.h"

namespace pdf_to_md {

/**
 * Detect printed fill-in blank horizontal lines on a page image and convert
 * them into synthetic OCR text boxes whose text is a run of '_'.
 *
 * Coordinates are in the same pixel space as @p page_bgr / existing OCR boxes.
 */
std::vector<medical_ocr::OcrTextBox> DetectBlankLines(
    const cv::Mat& page_bgr,
    const std::vector<medical_ocr::OcrTextBox>& existing_boxes,
    const ConvertOptions& options);

}  // namespace pdf_to_md

#endif  // PDF_TO_MD_BLANK_LINE_DETECTOR_H_
