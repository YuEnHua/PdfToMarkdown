#include "pdf_to_md/markdown_writer.h"

#include <fstream>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#endif

namespace pdf_to_md {
namespace {

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

std::string FileNameOnly(const std::string& path) {
    size_t pos = path.find_last_of("/\\");
    if (pos == std::string::npos) return path;
    return path.substr(pos + 1);
}

}  // namespace

std::string EscapeMarkdown(const std::string& text) {
    // Escape characters that commonly break plain paragraph Markdown.
    // Continuous underscore runs (fill-in blanks) are kept as-is so they
    // remain visible as ____ in Markdown viewers.
    std::string out;
    out.reserve(text.size() + 8);
    for (size_t i = 0; i < text.size();) {
        if (text[i] == '_') {
            size_t j = i;
            while (j < text.size() && text[j] == '_') ++j;
            const size_t run = j - i;
            if (run >= 2) {
                out.append(run, '_');
            } else {
                out.push_back('\\');
                out.push_back('_');
            }
            i = j;
            continue;
        }
        const char c = text[i++];
        switch (c) {
            case '\\':
            case '`':
            case '*':
            case '[':
            case ']':
            case '|':
                out.push_back('\\');
                out.push_back(c);
                break;
            default:
                out.push_back(c);
                break;
        }
    }
    return out;
}

std::string SanitizeHtmlComment(const std::string& text) {
    std::string out = text;
    // Prevent breaking out of HTML comments.
    for (size_t pos = 0; (pos = out.find("-->", pos)) != std::string::npos;) {
        out.replace(pos, 3, "->");
    }
    return out;
}

std::string BuildMarkdownDocument(const std::string& source_pdf_name,
                                  const std::vector<PageMarkdown>& pages) {
    std::ostringstream oss;
    oss << "<!-- generated-by: PdfToMarkdown 1.0.0 -->\n";
    oss << "<!-- source: " << SanitizeHtmlComment(FileNameOnly(source_pdf_name))
        << " -->\n\n";

    for (size_t i = 0; i < pages.size(); ++i) {
        const auto& page = pages[i];
        const int page_no = page.page_index + 1;
        oss << "<!-- page: " << page_no << " -->\n\n";
        if (page.paragraphs.empty()) {
            oss << "_（本页无识别到文字）_\n\n";
        } else {
            for (const auto& para : page.paragraphs) {
                // Escape line-by-line to preserve intentional newlines
                // inside a paragraph block.
                std::istringstream lines(para);
                std::string line;
                bool first = true;
                while (std::getline(lines, line)) {
                    if (!first) oss << "  \n";  // soft line break in MD
                    oss << EscapeMarkdown(line);
                    first = false;
                }
                oss << "\n\n";
            }
        }
        if (i + 1 < pages.size()) {
            oss << "---\n\n";
        }
    }
    return oss.str();
}

bool WriteMarkdownAtomic(const std::string& md_path_utf8,
                         const std::string& content,
                         std::string& error_out) {
    if (md_path_utf8.empty()) {
        error_out = "Markdown output path is empty";
        return false;
    }

    const std::string tmp_path = md_path_utf8 + ".tmp";

#ifdef _WIN32
    {
        std::wstring wtmp = Utf8ToWide(tmp_path);
        std::ofstream ofs(wtmp, std::ios::binary | std::ios::trunc);
        if (!ofs) {
            error_out = "Cannot create temporary markdown file (check permissions)";
            return false;
        }
        ofs.write(content.data(), static_cast<std::streamsize>(content.size()));
        if (!ofs) {
            error_out = "Failed writing temporary markdown file (disk full?)";
            return false;
        }
    }

    std::wstring wtmp = Utf8ToWide(tmp_path);
    std::wstring wdst = Utf8ToWide(md_path_utf8);
    if (!MoveFileExW(wtmp.c_str(), wdst.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        // Fallback: copy then delete
        if (!CopyFileW(wtmp.c_str(), wdst.c_str(), FALSE)) {
            error_out = "Failed to replace destination markdown file";
            DeleteFileW(wtmp.c_str());
            return false;
        }
        DeleteFileW(wtmp.c_str());
    }
#else
    {
        std::ofstream ofs(tmp_path, std::ios::binary | std::ios::trunc);
        if (!ofs) {
            error_out = "Cannot create temporary markdown file";
            return false;
        }
        ofs.write(content.data(), static_cast<std::streamsize>(content.size()));
        if (!ofs) {
            error_out = "Failed writing temporary markdown file";
            return false;
        }
    }
    if (std::rename(tmp_path.c_str(), md_path_utf8.c_str()) != 0) {
        error_out = "Failed to replace destination markdown file";
        return false;
    }
#endif

    error_out.clear();
    return true;
}

}  // namespace pdf_to_md
