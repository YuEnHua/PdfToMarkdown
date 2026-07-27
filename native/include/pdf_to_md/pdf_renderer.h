#ifndef PDF_TO_MD_PDF_RENDERER_H_
#define PDF_TO_MD_PDF_RENDERER_H_

#include <cstdint>
#include <string>
#include <vector>

#include <opencv2/core.hpp>

namespace pdf_to_md {

class PdfRenderer {
public:
    PdfRenderer();
    ~PdfRenderer();

    PdfRenderer(const PdfRenderer&) = delete;
    PdfRenderer& operator=(const PdfRenderer&) = delete;

    bool Open(const std::string& pdf_path_utf8);
    void Close();

    int PageCount() const { return page_count_; }
    bool IsEncrypted() const { return encrypted_; }
    std::string GetLastError() const { return last_error_; }

    /**
     * Render page (0-based) to BGR cv::Mat at the given DPI.
     * Does not write temporary image files.
     */
    bool RenderPage(int page_index, int dpi, cv::Mat& out_bgr);

private:
    void* document_ = nullptr;  // FPDF_DOCUMENT
    std::vector<uint8_t> file_bytes_;
    int page_count_ = 0;
    bool encrypted_ = false;
    bool library_held_ = false;
    std::string last_error_;

    static int library_refcount_;
};

}  // namespace pdf_to_md

#endif  // PDF_TO_MD_PDF_RENDERER_H_
