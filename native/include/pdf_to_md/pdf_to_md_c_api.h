#ifndef PDF_TO_MD_C_API_H_
#define PDF_TO_MD_C_API_H_

#include <stdint.h>

#include "pdf_to_md/error_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32) || defined(_WIN64)
#ifdef PDF_TO_MD_DLL_EXPORTS
#define PDFMD_API __declspec(dllexport)
#elif defined(__GNUC__)
#define PDFMD_API
#else
#define PDFMD_API __declspec(dllimport)
#endif
#else
#define PDFMD_API __attribute__((visibility("default")))
#endif

typedef struct PdfToMdHandleOpaque* PdfToMdHandle;

/**
 * Progress callback.
 * @param user_data  Opaque pointer passed to Convert.
 * @param current_page  1-based page currently finished (0 while starting).
 * @param total_pages   Total page count.
 * @param message_utf8  Short status (never contains OCR body text).
 */
typedef void (*PdfToMdProgressFn)(
    void* user_data,
    int current_page,
    int total_pages,
    const char* message_utf8);

/**
 * Create a converter handle.
 * @param models_dir_utf8  Directory containing PP-OCRv6_small_det/rec.
 * @param config_json_utf8 Optional JSON (dpi, cpu_threads, minimum_confidence).
 */
PDFMD_API int PdfToMd_Create(
    const char* models_dir_utf8,
    const char* config_json_utf8,
    PdfToMdHandle* out_handle);

PDFMD_API void PdfToMd_Destroy(PdfToMdHandle handle);

/**
 * Convert a PDF file to Markdown.
 * Thread safety: one Convert at a time per handle.
 */
PDFMD_API int PdfToMd_Convert(
    PdfToMdHandle handle,
    const char* pdf_path_utf8,
    const char* md_path_utf8,
    PdfToMdProgressFn progress_fn,
    void* user_data);

/** Request cancellation of an in-flight Convert. */
PDFMD_API void PdfToMd_Cancel(PdfToMdHandle handle);

PDFMD_API const char* PdfToMd_GetLastError(PdfToMdHandle handle);

PDFMD_API const char* PdfToMd_GetVersion(void);

/** Free a string returned by future APIs that allocate (reserved). */
PDFMD_API void PdfToMd_FreeString(char* str);

#ifdef __cplusplus
}
#endif

#endif  // PDF_TO_MD_C_API_H_
