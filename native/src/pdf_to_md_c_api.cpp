#include "pdf_to_md/pdf_to_md_c_api.h"

#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>

#include "pdf_to_md/converter.h"

struct PdfToMdHandleOpaque {
    pdf_to_md::Converter converter;
    std::string last_error;
};

namespace {

thread_local std::string g_last_error_no_handle;

void SetNoHandleError(const char* msg) {
    g_last_error_no_handle = msg ? msg : "";
}

}  // namespace

extern "C" {

PDFMD_API int PdfToMd_Create(const char* models_dir_utf8,
                             const char* config_json_utf8,
                             PdfToMdHandle* out_handle) {
    if (!out_handle) {
        SetNoHandleError("out_handle is null");
        return PDFMD_ERR_INVALID_ARG;
    }
    *out_handle = nullptr;
    if (!models_dir_utf8 || !*models_dir_utf8) {
        SetNoHandleError("models_dir is empty");
        return PDFMD_ERR_INVALID_ARG;
    }

    try {
        auto* h = new PdfToMdHandleOpaque();
        const std::string models = models_dir_utf8;
        const std::string cfg = config_json_utf8 ? config_json_utf8 : "";
        if (!h->converter.Initialize(models, cfg)) {
            h->last_error = h->converter.GetLastError();
            SetNoHandleError(h->last_error.c_str());
            const bool model_missing =
                h->last_error.find("missing") != std::string::npos ||
                h->last_error.find("model") != std::string::npos;
            delete h;
            return model_missing ? PDFMD_ERR_MODEL_MISSING : PDFMD_ERR_OCR_INIT;
        }
        *out_handle = h;
        return PDFMD_OK;
    } catch (const std::exception& e) {
        SetNoHandleError(e.what());
        return PDFMD_ERR_INTERNAL;
    } catch (...) {
        SetNoHandleError("Unknown exception in PdfToMd_Create");
        return PDFMD_ERR_INTERNAL;
    }
}

PDFMD_API void PdfToMd_Destroy(PdfToMdHandle handle) {
    if (!handle) return;
    try {
        handle->converter.Shutdown();
    } catch (...) {
    }
    delete handle;
}

PDFMD_API int PdfToMd_Convert(PdfToMdHandle handle,
                              const char* pdf_path_utf8,
                              const char* md_path_utf8,
                              PdfToMdProgressFn progress_fn,
                              void* user_data) {
    if (!handle) {
        SetNoHandleError("handle is null");
        return PDFMD_ERR_INVALID_ARG;
    }
    if (!pdf_path_utf8 || !md_path_utf8) {
        handle->last_error = "pdf/md path is null";
        return PDFMD_ERR_INVALID_ARG;
    }

    try {
        pdf_to_md::ProgressCallback cb;
        if (progress_fn) {
            cb = [progress_fn, user_data](int current, int total,
                                         const std::string& msg) {
                progress_fn(user_data, current, total, msg.c_str());
            };
        }

        const int rc =
            handle->converter.Convert(pdf_path_utf8, md_path_utf8, cb);
        handle->last_error = handle->converter.GetLastError();
        return rc;
    } catch (const std::exception& e) {
        handle->last_error = e.what();
        return PDFMD_ERR_INTERNAL;
    } catch (...) {
        handle->last_error = "Unknown exception in PdfToMd_Convert";
        return PDFMD_ERR_INTERNAL;
    }
}

PDFMD_API void PdfToMd_Cancel(PdfToMdHandle handle) {
    if (!handle) return;
    try {
        handle->converter.Cancel();
    } catch (...) {
    }
}

PDFMD_API const char* PdfToMd_GetLastError(PdfToMdHandle handle) {
    if (!handle) {
        return g_last_error_no_handle.c_str();
    }
    return handle->last_error.c_str();
}

PDFMD_API const char* PdfToMd_GetVersion(void) { return "1.0.0"; }

PDFMD_API void PdfToMd_FreeString(char* str) {
    if (str) {
        std::free(str);
    }
}

}  // extern "C"
