/**
 * PdfToMarkdown CLI
 *
 * Usage:
 *   PdfToMarkdown.Cli.exe input.pdf -o output.md --dpi 200 --models <dir>
 *   PdfToMarkdown.Cli.exe a.pdf b.pdf --models <dir>   # batch: sibling .md+.txt
 */

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

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
        "  PdfToMarkdown.Cli <a.pdf> [b.pdf ...] [options]\n"
        "Notes:\n"
        "  Always writes sibling .md and .txt (txt derived from md path).\n"
        "  Batch (multiple PDFs): omit -o; each output is beside the PDF.\n"
        "Options:\n"
        "  --models <dir>   PP-OCRv6 models directory\n"
        "  --dpi <n>        Render DPI (150-300, default 200)\n"
        "  --threads <n>    CPU threads (default 8)\n"
        "  -h, --help       Show help\n");
}

struct ProgressState {
    int last_page = -1;
    int file_index = 0;
    int file_count = 0;
};

void OnProgress(void* user, int current, int total, const char* message) {
    auto* st = static_cast<ProgressState*>(user);
    if (current != st->last_page) {
        st->last_page = current;
        if (st->file_count > 1) {
            std::printf("[file %d/%d] [%d/%d] %s\n", st->file_index,
                        st->file_count, current, total,
                        message ? message : "");
        } else {
            std::printf("[%d/%d] %s\n", current, total,
                        message ? message : "");
        }
        std::fflush(stdout);
    }
}

std::string DefaultModelsDir() { return "models"; }

std::string SuggestMdPath(const std::string& pdf) {
    const size_t slash = pdf.find_last_of("/\\");
    const size_t start = (slash == std::string::npos) ? 0 : slash + 1;
    const size_t dot = pdf.find_last_of('.');
    if (dot != std::string::npos && dot > start) {
        return pdf.substr(0, dot) + ".md";
    }
    return pdf + ".md";
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

    std::vector<std::string> inputs;
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
        inputs.push_back(a);
    }

    if (inputs.empty()) {
        PrintUsage();
        return 2;
    }

    if (inputs.size() > 1 && !output.empty()) {
        std::fprintf(stderr,
                     "Batch mode: omit -o; each PDF writes sibling .md/.txt\n");
        return 2;
    }

    if (inputs.size() == 1 && output.empty()) {
        output = SuggestMdPath(inputs[0]);
    }

    char config[256];
    std::snprintf(config, sizeof(config),
                  "{\"dpi\":%d,\"cpu_threads\":%d,\"enable_mkldnn\":false,"
                  "\"flush_each_page\":true}",
                  dpi, threads);

    PdfToMdHandle handle = nullptr;
    int rc = PdfToMd_Create(models.c_str(), config, &handle);
    if (rc != PDFMD_OK) {
        std::fprintf(stderr, "Init failed (%d): %s\n", rc,
                     PdfToMd_GetLastError(handle));
        return rc;
    }

    int ok = 0;
    int failed = 0;
    ProgressState st;
    st.file_count = static_cast<int>(inputs.size());

    for (size_t i = 0; i < inputs.size(); ++i) {
        st.file_index = static_cast<int>(i + 1);
        st.last_page = -1;
        const std::string md =
            (inputs.size() == 1) ? output : SuggestMdPath(inputs[i]);
        rc = PdfToMd_Convert(handle, inputs[i].c_str(), md.c_str(), OnProgress,
                             &st);
        if (rc != PDFMD_OK) {
            std::fprintf(stderr, "Convert failed (%d) %s: %s\n", rc,
                         inputs[i].c_str(), PdfToMd_GetLastError(handle));
            ++failed;
            if (rc == PDFMD_ERR_CANCELLED) break;
            continue;
        }
        std::printf("OK: %s (+ .txt)\n", md.c_str());
        ++ok;
    }

    PdfToMd_Destroy(handle);
    if (failed > 0 && ok == 0) return rc != PDFMD_OK ? rc : 1;
    if (failed > 0) return 1;
    return 0;
}
