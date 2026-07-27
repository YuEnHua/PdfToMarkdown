#include <gtest/gtest.h>

#include "medical_ocr/types.h"
#include "pdf_to_md/reading_order.h"

using medical_ocr::OcrResult;
using medical_ocr::OcrTextBox;
using medical_ocr::PointF;

static OcrTextBox MakeBox(float x, float y, float w, float h,
                          const std::string& text, float conf = 0.9f) {
    OcrTextBox b;
    b.text = text;
    b.confidence = conf;
    b.points = {PointF{x, y}, PointF{x + w, y}, PointF{x + w, y + h},
                PointF{x, y + h}};
    return b;
}

TEST(ReadingOrder, SortsLeftToRightSameLine) {
    OcrResult ocr;
    ocr.imageWidth = 800;
    ocr.imageHeight = 600;
    ocr.boxes.push_back(MakeBox(200, 100, 80, 20, "World"));
    ocr.boxes.push_back(MakeBox(50, 102, 80, 20, "Hello"));

    auto lines = pdf_to_md::BuildReadingLines(ocr, 0.6f);
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_EQ(lines[0].text, "Hello World");
}

TEST(ReadingOrder, MultipleLinesTopToBottom) {
    OcrResult ocr;
    ocr.imageWidth = 800;
    ocr.imageHeight = 600;
    ocr.boxes.push_back(MakeBox(50, 200, 100, 20, "Line2"));
    ocr.boxes.push_back(MakeBox(50, 50, 100, 20, "Line1"));

    auto lines = pdf_to_md::BuildReadingLines(ocr, 0.6f);
    ASSERT_EQ(lines.size(), 2u);
    EXPECT_EQ(lines[0].text, "Line1");
    EXPECT_EQ(lines[1].text, "Line2");
}

TEST(ReadingOrder, ParagraphMergeByGap) {
    std::vector<pdf_to_md::TextLine> lines(3);
    lines[0].y_center = 10;
    lines[0].height = 16;
    lines[0].text = "A1";
    lines[1].y_center = 28;
    lines[1].height = 16;
    lines[1].text = "A2";
    lines[2].y_center = 120;
    lines[2].height = 16;
    lines[2].text = "B1";

    auto paras = pdf_to_md::MergeParagraphs(lines, 1.8f);
    ASSERT_EQ(paras.size(), 2u);
    EXPECT_NE(paras[0].find("A1"), std::string::npos);
    EXPECT_NE(paras[0].find("A2"), std::string::npos);
    EXPECT_EQ(paras[1], "B1");
}

TEST(ReadingOrder, EmptyInput) {
    OcrResult ocr;
    auto lines = pdf_to_md::BuildReadingLines(ocr);
    EXPECT_TRUE(lines.empty());
    auto page = pdf_to_md::PageToMarkdown(0, ocr, pdf_to_md::ConvertOptions{});
    EXPECT_TRUE(page.paragraphs.empty());
}
