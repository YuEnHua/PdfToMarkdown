#include <gtest/gtest.h>

#include <opencv2/imgproc.hpp>

#include "pdf_to_md/blank_line_detector.h"
#include "pdf_to_md/reading_order.h"

using medical_ocr::OcrTextBox;
using medical_ocr::PointF;

static OcrTextBox MakeTextBox(float x, float y, float w, float h,
                              const std::string& text) {
    OcrTextBox b;
    b.text = text;
    b.confidence = 0.95f;
    b.points = {PointF{x, y}, PointF{x + w, y}, PointF{x + w, y + h},
                PointF{x, y + h}};
    return b;
}

TEST(BlankLineDetector, DetectsShortAndLongBlanks) {
    // White page with two short blanks and one long writing line.
    cv::Mat page(800, 1000, CV_8UC3, cv::Scalar(255, 255, 255));

    // Short blanks farther apart so they stay separate fill-ins.
    cv::line(page, cv::Point(80, 120), cv::Point(160, 120), cv::Scalar(0, 0, 0),
             2);
    cv::line(page, cv::Point(280, 120), cv::Point(380, 120), cv::Scalar(0, 0, 0),
             2);

    // Long writing line under a "sentence".
    cv::line(page, cv::Point(60, 400), cv::Point(920, 400), cv::Scalar(0, 0, 0),
             2);

    std::vector<OcrTextBox> existing;
    existing.push_back(MakeTextBox(40, 90, 30, 28, "There"));
    existing.push_back(MakeTextBox(320, 90, 80, 28, "wrong"));
    existing.push_back(MakeTextBox(60, 340, 400, 36, "翻译句子示例"));

    pdf_to_md::ConvertOptions opt;
    opt.dpi = 200;
    opt.enable_blank_line_detection = true;
    opt.blank_min_width_px = 40;
    opt.blank_max_thickness_px = 6;
    opt.blank_long_width_ratio = 0.45f;

    auto blanks = pdf_to_md::DetectBlankLines(page, existing, opt);
    ASSERT_GE(blanks.size(), 3u);

    int shortish = 0;
    int longish = 0;
    for (const auto& b : blanks) {
        ASSERT_FALSE(b.text.empty());
        EXPECT_EQ(b.text.find_first_not_of('_'), std::string::npos);
        EXPECT_GE(b.text.size(), 3u);
        if (b.text.size() >= 20) {
            ++longish;
        } else {
            ++shortish;
        }
    }
    EXPECT_GE(shortish, 2);
    EXPECT_GE(longish, 1);
}

TEST(BlankLineDetector, DisabledReturnsEmpty) {
    cv::Mat page(200, 400, CV_8UC3, cv::Scalar(255, 255, 255));
    cv::line(page, cv::Point(20, 100), cv::Point(380, 100), cv::Scalar(0, 0, 0),
             2);
    pdf_to_md::ConvertOptions opt;
    opt.enable_blank_line_detection = false;
    auto blanks = pdf_to_md::DetectBlankLines(page, {}, opt);
    EXPECT_TRUE(blanks.empty());
}

TEST(BlankLineDetector, MergesIntoReadingOrder) {
    medical_ocr::OcrResult ocr;
    ocr.imageWidth = 600;
    ocr.imageHeight = 200;
    ocr.boxes.push_back(MakeTextBox(20, 40, 60, 24, "There"));
    // Synthetic blank between words.
    OcrTextBox blank = MakeTextBox(100, 52, 80, 4, "______");
    ocr.boxes.push_back(blank);
    ocr.boxes.push_back(MakeTextBox(200, 40, 80, 24, "wrong"));

    auto page = pdf_to_md::PageToMarkdown(0, ocr, pdf_to_md::ConvertOptions{});
    ASSERT_FALSE(page.paragraphs.empty());
    EXPECT_NE(page.paragraphs[0].find("There"), std::string::npos);
    EXPECT_NE(page.paragraphs[0].find("______"), std::string::npos);
    EXPECT_NE(page.paragraphs[0].find("wrong"), std::string::npos);
}
