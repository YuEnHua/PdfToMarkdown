#include "pdf_to_md/converter.h"

#include <opencv2/core.hpp>

#include "nlohmann/json.hpp"

#include "pdf_to_md/error_codes.h"
#include "pdf_to_md/blank_line_detector.h"
#include "pdf_to_md/markdown_writer.h"
#include "pdf_to_md/pdf_renderer.h"
#include "pdf_to_md/reading_order.h"
#include "src/ocr/paddle_ocr_engine.h"

namespace pdf_to_md {

Converter::Converter() = default;

Converter::~Converter() { Shutdown(); }

void Converter::SetError(const std::string& msg) {
    std::lock_guard<std::mutex> lock(error_mutex_);
    last_error_ = msg;
}

std::string Converter::GetLastError() const {
    std::lock_guard<std::mutex> lock(error_mutex_);
    return last_error_;
}

bool Converter::Initialize(const std::string& models_dir_utf8,
                           const std::string& config_json_utf8) {
    if (initialized_) {
        SetError("Already initialized");
        return false;
    }
    if (models_dir_utf8.empty()) {
        SetError("Models directory is empty");
        return false;
    }

    options_ = ConvertOptions{};
    nlohmann::json engine_cfg;
    engine_cfg["enable_mkldnn"] = false;
    engine_cfg["cpu_threads"] = options_.cpu_threads;
    engine_cfg["minimum_confidence"] = options_.minimum_confidence;
    engine_cfg["det_model_name"] = "PP-OCRv6_small_det";
    engine_cfg["rec_model_name"] = "PP-OCRv6_small_rec";

    if (!config_json_utf8.empty()) {
        try {
            auto cfg = nlohmann::json::parse(config_json_utf8);
            options_.dpi = cfg.value("dpi", options_.dpi);
            options_.cpu_threads = cfg.value("cpu_threads", options_.cpu_threads);
            options_.minimum_confidence =
                cfg.value("minimum_confidence", options_.minimum_confidence);
            options_.line_y_tolerance_ratio = cfg.value(
                "line_y_tolerance_ratio", options_.line_y_tolerance_ratio);
            options_.paragraph_gap_ratio = cfg.value(
                "paragraph_gap_ratio", options_.paragraph_gap_ratio);
            options_.enable_blank_line_detection = cfg.value(
                "enable_blank_line_detection",
                options_.enable_blank_line_detection);
            options_.blank_min_width_px =
                cfg.value("blank_min_width_px", options_.blank_min_width_px);
            options_.blank_max_thickness_px = cfg.value(
                "blank_max_thickness_px", options_.blank_max_thickness_px);
            options_.blank_long_width_ratio = cfg.value(
                "blank_long_width_ratio", options_.blank_long_width_ratio);
            options_.enable_column_detection = cfg.value(
                "enable_column_detection", options_.enable_column_detection);
            options_.column_gap_min_ratio = cfg.value(
                "column_gap_min_ratio", options_.column_gap_min_ratio);
            options_.column_min_boxes_per_side = cfg.value(
                "column_min_boxes_per_side",
                options_.column_min_boxes_per_side);
            engine_cfg["cpu_threads"] = options_.cpu_threads;
            engine_cfg["minimum_confidence"] = options_.minimum_confidence;
            engine_cfg["enable_mkldnn"] = cfg.value("enable_mkldnn", false);
        } catch (const std::exception& e) {
            SetError(std::string("Config JSON parse error: ") + e.what());
            return false;
        }
    }

    if (options_.dpi < 150) options_.dpi = 150;
    if (options_.dpi > 300) options_.dpi = 300;

    engine_ = std::make_unique<medical_ocr::PaddleOcrEngine>();
    if (!engine_->Initialize(models_dir_utf8, engine_cfg.dump())) {
        SetError(engine_->GetLastError());
        engine_.reset();
        return false;
    }

    initialized_ = true;
    SetError("");
    return true;
}

void Converter::Shutdown() {
    cancel_requested_ = true;
    if (engine_) {
        engine_->Shutdown();
        engine_.reset();
    }
    initialized_ = false;
}

void Converter::Cancel() { cancel_requested_ = true; }

int Converter::Convert(const std::string& pdf_path_utf8,
                       const std::string& md_path_utf8,
                       ProgressCallback progress) {
    if (!initialized_ || !engine_) {
        SetError("Converter not initialized");
        return PDFMD_ERR_NOT_INITIALIZED;
    }
    if (pdf_path_utf8.empty() || md_path_utf8.empty()) {
        SetError("PDF or Markdown path is empty");
        return PDFMD_ERR_INVALID_ARG;
    }

    bool expected = false;
    if (!busy_.compare_exchange_strong(expected, true)) {
        SetError("Converter is busy");
        return PDFMD_ERR_BUSY;
    }

    cancel_requested_ = false;
    int rc = PDFMD_OK;

    try {
        if (progress) progress(0, 0, "Opening PDF");

        PdfRenderer renderer;
        if (!renderer.Open(pdf_path_utf8)) {
            SetError(renderer.GetLastError());
            rc = renderer.IsEncrypted() ? PDFMD_ERR_PDF_ENCRYPTED
                                        : PDFMD_ERR_PDF_OPEN;
            if (renderer.PageCount() == 0 &&
                renderer.GetLastError().find("zero") != std::string::npos) {
                rc = PDFMD_ERR_PDF_EMPTY;
            }
            busy_ = false;
            return rc;
        }

        const int total = renderer.PageCount();
        if (total <= 0) {
            SetError("PDF has zero pages");
            busy_ = false;
            return PDFMD_ERR_PDF_EMPTY;
        }

        if (progress) progress(0, total, "PDF opened");

        std::vector<PageMarkdown> pages;
        pages.reserve(static_cast<size_t>(total));

        for (int i = 0; i < total; ++i) {
            if (cancel_requested_) {
                SetError("Cancelled by user");
                busy_ = false;
                return PDFMD_ERR_CANCELLED;
            }

            if (progress) {
                progress(i, total,
                         "Rendering page " + std::to_string(i + 1));
            }

            cv::Mat page_bgr;
            if (!renderer.RenderPage(i, options_.dpi, page_bgr)) {
                SetError(renderer.GetLastError());
                busy_ = false;
                return PDFMD_ERR_PDF_RENDER;
            }

            if (cancel_requested_) {
                SetError("Cancelled by user");
                busy_ = false;
                return PDFMD_ERR_CANCELLED;
            }

            if (progress) {
                progress(i, total, "OCR page " + std::to_string(i + 1));
            }

            medical_ocr::OcrResult ocr;
#ifdef MEDICAL_OCR_HAS_PADDLE
            if (!engine_->RecognizeMat(page_bgr, ocr)) {
                SetError(engine_->GetLastError());
                busy_ = false;
                return PDFMD_ERR_OCR_FAILED;
            }
#else
            (void)page_bgr;
            SetError("Built without Paddle OCR support");
            busy_ = false;
            return PDFMD_ERR_OCR_INIT;
#endif

            if (options_.enable_blank_line_detection) {
                auto blanks =
                    DetectBlankLines(page_bgr, ocr.boxes, options_);
                ocr.boxes.insert(ocr.boxes.end(), blanks.begin(), blanks.end());
            }

            pages.push_back(PageToMarkdown(i, ocr, options_));

            if (progress) {
                progress(i + 1, total,
                         "Finished page " + std::to_string(i + 1));
            }
        }

        if (cancel_requested_) {
            SetError("Cancelled by user");
            busy_ = false;
            return PDFMD_ERR_CANCELLED;
        }

        if (progress) progress(total, total, "Writing Markdown");

        const std::string md =
            BuildMarkdownDocument(pdf_path_utf8, pages);
        std::string write_err;
        if (!WriteMarkdownAtomic(md_path_utf8, md, write_err)) {
            SetError(write_err);
            busy_ = false;
            return PDFMD_ERR_WRITE_FAILED;
        }

        SetError("");
        if (progress) progress(total, total, "Done");
        rc = PDFMD_OK;
    } catch (const std::exception& e) {
        SetError(std::string("Internal error: ") + e.what());
        rc = PDFMD_ERR_INTERNAL;
    }

    busy_ = false;
    return rc;
}

}  // namespace pdf_to_md
