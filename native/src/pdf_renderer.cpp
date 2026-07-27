#include "pdf_to_md/pdf_renderer.h"

#include <algorithm>
#include <mutex>

#ifdef _WIN32
#include <windows.h>
#endif

#include <opencv2/imgproc.hpp>

#include "fpdfview.h"

namespace pdf_to_md {
namespace {

std::mutex g_pdfium_mutex;

std::wstring Utf8ToWide(const std::string& utf8) {
#ifdef _WIN32
    if (utf8.empty()) return {};
    int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    if (wlen <= 0) return {};
    std::wstring wide(static_cast<size_t>(wlen - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &wide[0], wlen);
    return wide;
#else
    (void)utf8;
    return {};
#endif
}

bool ReadFileUtf8(const std::string& path_utf8, std::vector<uint8_t>& out,
                  std::string& error) {
    out.clear();
#ifdef _WIN32
    std::wstring wpath = Utf8ToWide(path_utf8);
    HANDLE file = CreateFileW(wpath.c_str(), GENERIC_READ, FILE_SHARE_READ,
                              nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                              nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        error = "Failed to open PDF file (path not found or inaccessible)";
        return false;
    }
    LARGE_INTEGER size_li = {};
    if (!GetFileSizeEx(file, &size_li) || size_li.QuadPart <= 0) {
        CloseHandle(file);
        error = "Invalid PDF file size";
        return false;
    }
    if (size_li.QuadPart > 512LL * 1024 * 1024) {
        CloseHandle(file);
        error = "PDF file too large (>512MB)";
        return false;
    }
    const DWORD size = static_cast<DWORD>(size_li.QuadPart);
    out.resize(size);
    DWORD read = 0;
    BOOL ok = ReadFile(file, out.data(), size, &read, nullptr);
    CloseHandle(file);
    if (!ok || read != size) {
        out.clear();
        error = "Failed to read PDF file";
        return false;
    }
    return true;
#else
    FILE* fp = fopen(path_utf8.c_str(), "rb");
    if (!fp) {
        error = "Failed to open PDF file";
        return false;
    }
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (sz <= 0) {
        fclose(fp);
        error = "Invalid PDF file size";
        return false;
    }
    out.resize(static_cast<size_t>(sz));
    size_t n = fread(out.data(), 1, out.size(), fp);
    fclose(fp);
    if (n != out.size()) {
        out.clear();
        error = "Failed to read PDF file";
        return false;
    }
    return true;
#endif
}

}  // namespace

int PdfRenderer::library_refcount_ = 0;

PdfRenderer::PdfRenderer() = default;

PdfRenderer::~PdfRenderer() { Close(); }

bool PdfRenderer::Open(const std::string& pdf_path_utf8) {
    Close();
    encrypted_ = false;
    last_error_.clear();

    if (pdf_path_utf8.empty()) {
        last_error_ = "PDF path is empty";
        return false;
    }

    if (!ReadFileUtf8(pdf_path_utf8, file_bytes_, last_error_)) {
        return false;
    }

    std::lock_guard<std::mutex> lock(g_pdfium_mutex);
    if (library_refcount_ == 0) {
        FPDF_LIBRARY_CONFIG config = {};
        config.version = 2;
        config.m_pUserFontPaths = nullptr;
        config.m_pIsolate = nullptr;
        config.m_v8EmbedderSlot = 0;
        FPDF_InitLibraryWithConfig(&config);
    }
    ++library_refcount_;
    library_held_ = true;

    FPDF_DOCUMENT doc = FPDF_LoadMemDocument(
        file_bytes_.data(), static_cast<int>(file_bytes_.size()), nullptr);

    if (!doc) {
        const unsigned long err = FPDF_GetLastError();
        if (err == FPDF_ERR_PASSWORD) {
            encrypted_ = true;
            last_error_ = "PDF is encrypted / password protected";
        } else if (err == FPDF_ERR_FORMAT) {
            last_error_ = "PDF format error / corrupted file";
        } else if (err == FPDF_ERR_FILE) {
            last_error_ = "PDF file could not be opened";
        } else {
            last_error_ = "Failed to load PDF (error " + std::to_string(err) + ")";
        }
        file_bytes_.clear();
        --library_refcount_;
        library_held_ = false;
        if (library_refcount_ == 0) {
            FPDF_DestroyLibrary();
        }
        return false;
    }

    document_ = doc;
    page_count_ = FPDF_GetPageCount(doc);
    if (page_count_ <= 0) {
        last_error_ = "PDF has zero pages";
        // Close without double-lock: manually tear down under same lock.
        FPDF_CloseDocument(doc);
        document_ = nullptr;
        file_bytes_.clear();
        page_count_ = 0;
        --library_refcount_;
        library_held_ = false;
        if (library_refcount_ == 0) {
            FPDF_DestroyLibrary();
        }
        return false;
    }
    return true;
}

void PdfRenderer::Close() {
    std::lock_guard<std::mutex> lock(g_pdfium_mutex);
    if (document_) {
        FPDF_CloseDocument(static_cast<FPDF_DOCUMENT>(document_));
        document_ = nullptr;
    }
    file_bytes_.clear();
    page_count_ = 0;
    if (library_held_) {
        library_held_ = false;
        if (library_refcount_ > 0) {
            --library_refcount_;
            if (library_refcount_ == 0) {
                FPDF_DestroyLibrary();
            }
        }
    }
}

bool PdfRenderer::RenderPage(int page_index, int dpi, cv::Mat& out_bgr) {
    out_bgr.release();
    if (!document_) {
        last_error_ = "PDF not open";
        return false;
    }
    if (page_index < 0 || page_index >= page_count_) {
        last_error_ = "Page index out of range";
        return false;
    }
    if (dpi < 72 || dpi > 600) {
        last_error_ = "DPI out of supported range (72-600)";
        return false;
    }

    std::lock_guard<std::mutex> lock(g_pdfium_mutex);
    FPDF_DOCUMENT doc = static_cast<FPDF_DOCUMENT>(document_);
    FPDF_PAGE page = FPDF_LoadPage(doc, page_index);
    if (!page) {
        last_error_ = "Failed to load PDF page";
        return false;
    }

    const double page_width_pt = FPDF_GetPageWidth(page);
    const double page_height_pt = FPDF_GetPageHeight(page);
    const double scale = static_cast<double>(dpi) / 72.0;
    int width = static_cast<int>(page_width_pt * scale + 0.5);
    int height = static_cast<int>(page_height_pt * scale + 0.5);
    if (width < 1) width = 1;
    if (height < 1) height = 1;

    const int max_side = 8000;
    if (width > max_side || height > max_side) {
        const double clamp = static_cast<double>(max_side) /
                             static_cast<double>((std::max)(width, height));
        width = static_cast<int>(width * clamp);
        height = static_cast<int>(height * clamp);
    }

    FPDF_BITMAP bitmap = FPDFBitmap_Create(width, height, 1);
    if (!bitmap) {
        FPDF_ClosePage(page);
        last_error_ = "Failed to create PDF bitmap";
        return false;
    }

    FPDFBitmap_FillRect(bitmap, 0, 0, width, height, 0xFFFFFFFF);
    FPDF_RenderPageBitmap(bitmap, page, 0, 0, width, height, 0, FPDF_ANNOT);

    void* buffer = FPDFBitmap_GetBuffer(bitmap);
    const int stride = FPDFBitmap_GetStride(bitmap);
    if (!buffer || stride <= 0) {
        FPDFBitmap_Destroy(bitmap);
        FPDF_ClosePage(page);
        last_error_ = "Invalid PDF bitmap buffer";
        return false;
    }

    cv::Mat bgra(height, width, CV_8UC4, buffer, stride);
    cv::Mat bgr;
    cv::cvtColor(bgra, bgr, cv::COLOR_BGRA2BGR);
    out_bgr = bgr.clone();

    FPDFBitmap_Destroy(bitmap);
    FPDF_ClosePage(page);
    last_error_.clear();
    return true;
}

}  // namespace pdf_to_md
