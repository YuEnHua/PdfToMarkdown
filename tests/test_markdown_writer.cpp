#include <gtest/gtest.h>

#include <cstdio>
#include <fstream>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

#include "pdf_to_md/markdown_writer.h"
#include "pdf_to_md/types.h"

TEST(MarkdownWriter, EscapesSpecialChars) {
    EXPECT_EQ(pdf_to_md::EscapeMarkdown("a*b_c"), "a\\*b\\_c");
    EXPECT_EQ(pdf_to_md::EscapeMarkdown("plain"), "plain");
}

TEST(MarkdownWriter, PreservesFillInUnderscoreRuns) {
    EXPECT_EQ(pdf_to_md::EscapeMarkdown("There ______ wrong"),
              "There ______ wrong");
    EXPECT_EQ(pdf_to_md::EscapeMarkdown("a_b"), "a\\_b");
}

TEST(MarkdownWriter, BuildsPagesWithMarkers) {
    std::vector<pdf_to_md::PageMarkdown> pages(2);
    pages[0].page_index = 0;
    pages[0].paragraphs = {"第一段"};
    pages[1].page_index = 1;
    pages[1].paragraphs = {"第二段"};

    const std::string md =
        pdf_to_md::BuildMarkdownDocument("scan.pdf", pages);
    EXPECT_NE(md.find("<!-- page: 1 -->"), std::string::npos);
    EXPECT_NE(md.find("<!-- page: 2 -->"), std::string::npos);
    EXPECT_NE(md.find("---"), std::string::npos);
    EXPECT_NE(md.find("第一段"), std::string::npos);
    EXPECT_NE(md.find("第二段"), std::string::npos);
}

TEST(MarkdownWriter, BuildsPlainTextWithoutEscapes) {
    std::vector<pdf_to_md::PageMarkdown> pages(2);
    pages[0].page_index = 0;
    pages[0].paragraphs = {"Hello *world*"};
    pages[1].page_index = 1;
    pages[1].paragraphs = {"第二段"};

    const std::string txt =
        pdf_to_md::BuildPlainTextDocument("scan.pdf", pages);
    EXPECT_NE(txt.find("----- page 1 -----"), std::string::npos);
    EXPECT_NE(txt.find("----- page 2 -----"), std::string::npos);
    EXPECT_NE(txt.find("Hello *world*"), std::string::npos);
    EXPECT_EQ(txt.find("\\*"), std::string::npos);
    EXPECT_NE(txt.find("第二段"), std::string::npos);
    EXPECT_EQ(txt.find("<!--"), std::string::npos);
}

TEST(MarkdownWriter, DeriveTxtPathFromMd) {
    EXPECT_EQ(pdf_to_md::DeriveTxtPathFromMdPath("C:\\out\\scan.md"),
              "C:\\out\\scan.txt");
    EXPECT_EQ(pdf_to_md::DeriveTxtPathFromMdPath("/tmp/a.MD"), "/tmp/a.txt");
    EXPECT_EQ(pdf_to_md::DeriveTxtPathFromMdPath("noext"), "noext.txt");
}

TEST(MarkdownWriter, AtomicWriteRoundTrip) {
#ifdef _WIN32
    wchar_t temp_dir[MAX_PATH] = {};
    GetTempPathW(MAX_PATH, temp_dir);
    std::wstring wpath = std::wstring(temp_dir) + L"pdfmd_test_out.md";
    char path_u8[MAX_PATH * 3] = {};
    WideCharToMultiByte(CP_UTF8, 0, wpath.c_str(), -1, path_u8,
                        static_cast<int>(sizeof(path_u8)), nullptr, nullptr);
    std::string path = path_u8;
#else
    std::string path = "/tmp/pdfmd_test_out.md";
#endif

    std::string err;
    ASSERT_TRUE(pdf_to_md::WriteMarkdownAtomic(path, "hello-md\n", err)) << err;

    std::ifstream ifs(path, std::ios::binary);
    ASSERT_TRUE(ifs.good());
    std::string content((std::istreambuf_iterator<char>(ifs)),
                        std::istreambuf_iterator<char>());
    EXPECT_EQ(content, "hello-md\n");

#ifdef _WIN32
    DeleteFileW(wpath.c_str());
#else
    std::remove(path.c_str());
#endif
}
