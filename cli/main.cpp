/**
 * PdfToMarkdown CLI
 *
 * Usage:
 *   PdfToMarkdown.Cli.exe input.pdf -o output.md --dpi 200 --models <dir>
 */

#include <cstdio>
#include <cstring>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

#include "pdf_to_md/pdf_to_md_c_api.h"

namespace {

void PrintUsage() {
    std::printf(
        "PdfToMarkdown CLI 1.0.0\n"
        "Usage:\n"
        "  PdfToMarkdown.Cli <input.pdf> -o <output.md> [options]\n"
        "Options:\n"
        "  --models <dir>   PP-OCRv6 models directory\n"
        "  --dpi <n>        Render DPI (150-300, default 200)\n"
        "  --threads <n>    CPU threads (default 8)\n"
        "  -h, --help       Show help\n");
}

struct ProgressState {
    int last_page = -1;
};

void OnProgress(void* user, int current, int total, const char* message) {
    auto* st = static_cast<ProgressState*>(user);
    if (current != st->last_page) {
        st->last_page = current;
        std::printf("[%d/%d] %s\n", current, total, message ? message : "");
        std::fflush(stdout);
    }
}

std::string DefaultModelsDir() {
    // Prefer sibling models from MedicalOCR, then Environment.
    return "models";
}

}  // namespace

int main(int argc, char** argv) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    if (argc < 2) {
        PrintUsage();
        return 2;
    }

    std::string input;
    std::string output;
    std::string models = DefaultModelsDir();
    int dpi = 200;
    int threads = 8;

    for (int i = 1; i < argc; ++i) {
        const char* a = argv[i];
        if (std::strcmp(a, "-h") == 0 || std::strcmp(a, "--help") == 0) {
            PrintUsage();
            return 0;
        }
        if (std::strcmp(a, "-o") == 0 && i + 1 < argc) {
            output = argv[++i];
            continue;
        }
        if (std::strcmp(a, "--models") == 0 && i + 1 < argc) {
            models = argv[++i];
            continue;
        }
        if (std::strcmp(a, "--dpi") == 0 && i + 1 < argc) {
            dpi = std::atoi(argv[++i]);
            continue;
        }
        if (std::strcmp(a, "--threads") == 0 && i + 1 < argc) {
            threads = std::atoi(argv[++i]);
            continue;
        }
        if (a[0] == '-') {
            std::fprintf(stderr, "Unknown option: %s\n", a);
            return 2;
        }
        if (input.empty()) {
            input = a;
        } else {
            std::fprintf(stderr, "Unexpected argument: %s\n", a);
            return 2;
        }
    }

    if (input.empty() || output.empty()) {
        PrintUsage();
        return 2;
    }

    char config[256];
    std::snprintf(config, sizeof(config),
                  "{\"dpi\":%d,\"cpu_threads\":%d,\"enable_mkldnn\":false}", dpi,
                  threads);

    PdfToMdHandle handle = nullptr;
    int rc = PdfToMd_Create(models.c_str(), config, &handle);
    if (rc != PDFMD_OK) {
        std::fprintf(stderr, "Init failed (%d): %s\n", rc,
                     PdfToMd_GetLastError(handle));
        return rc;
    }

    ProgressState st;
    rc = PdfToMd_Convert(handle, input.c_str(), output.c_str(), OnProgress, &st);
    if (rc != PDFMD_OK) {
        std::fprintf(stderr, "Convert failed (%d): %s\n", rc,
                     PdfToMd_GetLastError(handle));
        PdfToMd_Destroy(handle);
        return rc;
    }

    std::printf("OK: %s\n", output.c_str());
    PdfToMd_Destroy(handle);
    return 0;
}
