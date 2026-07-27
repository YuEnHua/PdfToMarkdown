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

TEST(ReadingOrder, TwoColumnReadsLeftThenRight) {
    // Left column at x~50, right at x~500, large gap in middle (image).
    OcrResult ocr;
    ocr.imageWidth = 800;
    ocr.imageHeight = 600;
    ocr.boxes.push_back(MakeBox(40, 40, 120, 20, "L1"));
    ocr.boxes.push_back(MakeBox(40, 80, 120, 20, "L2"));
    ocr.boxes.push_back(MakeBox(520, 40, 120, 20, "R1"));
    ocr.boxes.push_back(MakeBox(520, 80, 120, 20, "R2"));

    pdf_to_md::ConvertOptions opt;
    opt.enable_column_detection = true;
    opt.column_gap_min_ratio = 0.10f;
    opt.column_min_boxes_per_side = 2;

    float split = 0.0f;
    ASSERT_TRUE(pdf_to_md::FindColumnSplitX(ocr.boxes, ocr.imageWidth, opt,
                                            split));
    EXPECT_GT(split, 200.0f);
    EXPECT_LT(split, 500.0f);

    auto page = pdf_to_md::PageToMarkdown(0, ocr, opt);
    ASSERT_FALSE(page.paragraphs.empty());

    // Flatten for order check: left content must appear before right.
    std::string all;
    for (const auto& p : page.paragraphs) {
        all += p;
        all += "\n";
    }
    const auto pos_l1 = all.find("L1");
    const auto pos_l2 = all.find("L2");
    const auto pos_r1 = all.find("R1");
    const auto pos_r2 = all.find("R2");
    ASSERT_NE(pos_l1, std::string::npos);
    ASSERT_NE(pos_l2, std::string::npos);
    ASSERT_NE(pos_r1, std::string::npos);
    ASSERT_NE(pos_r2, std::string::npos);
    EXPECT_LT(pos_l1, pos_r1);
    EXPECT_LT(pos_l2, pos_r1);
    EXPECT_LT(pos_l1, pos_r2);

    // Must NOT merge same-Y left+right into one line.
    EXPECT_EQ(all.find("L1 R1"), std::string::npos);
    EXPECT_EQ(all.find("L2 R2"), std::string::npos);
}

TEST(ReadingOrder, ImageAboveTextStaysSingleColumn) {
    // Text only in lower half spanning full width — no left/right gap.
    OcrResult ocr;
    ocr.imageWidth = 800;
    ocr.imageHeight = 600;
    ocr.boxes.push_back(MakeBox(40, 350, 200, 20, "ParaA"));
    ocr.boxes.push_back(MakeBox(260, 350, 200, 20, "continues"));
    ocr.boxes.push_back(MakeBox(40, 400, 300, 20, "ParaB"));

    pdf_to_md::ConvertOptions opt;
    opt.enable_column_detection = true;
    opt.column_gap_min_ratio = 0.10f;
    opt.column_min_boxes_per_side = 2;

    float split = 0.0f;
    EXPECT_FALSE(pdf_to_md::FindColumnSplitX(ocr.boxes, ocr.imageWidth, opt,
                                             split));

    auto page = pdf_to_md::PageToMarkdown(0, ocr, opt);
    ASSERT_FALSE(page.paragraphs.empty());
    // Same line should still join with a space (single column).
    bool found_joined = false;
    for (const auto& p : page.paragraphs) {
        if (p.find("ParaA continues") != std::string::npos) found_joined = true;
    }
    EXPECT_TRUE(found_joined);
}

TEST(ReadingOrder, ColumnDetectionCanDisable) {
    OcrResult ocr;
    ocr.imageWidth = 800;
    ocr.imageHeight = 600;
    ocr.boxes.push_back(MakeBox(40, 40, 120, 20, "L1"));
    ocr.boxes.push_back(MakeBox(40, 80, 120, 20, "L2"));
    ocr.boxes.push_back(MakeBox(520, 40, 120, 20, "R1"));
    ocr.boxes.push_back(MakeBox(520, 80, 120, 20, "R2"));

    pdf_to_md::ConvertOptions opt;
    opt.enable_column_detection = false;

    float split = 0.0f;
    EXPECT_FALSE(pdf_to_md::FindColumnSplitX(ocr.boxes, ocr.imageWidth, opt,
                                             split));

    // With detection off, same-Y boxes merge: "L1 R1".
    auto page = pdf_to_md::PageToMarkdown(0, ocr, opt);
    std::string all;
    for (const auto& p : page.paragraphs) all += p + "\n";
    EXPECT_NE(all.find("L1 R1"), std::string::npos);
}
