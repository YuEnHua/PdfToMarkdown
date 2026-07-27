#ifndef PDF_TO_MD_MARKDOWN_WRITER_H_
#define PDF_TO_MD_MARKDOWN_WRITER_H_

#include <string>
#include <vector>

#include "pdf_to_md/types.h"

namespace pdf_to_md {

std::string EscapeMarkdown(const std::string& text);

std::string BuildMarkdownDocument(
    const std::string& source_pdf_name,
    const std::vector<PageMarkdown>& pages);

/**
 * Write UTF-8 Markdown (no BOM) via temp file then atomic replace.
 */
bool WriteMarkdownAtomic(
    const std::string& md_path_utf8,
    const std::string& content,
    std::string& error_out);

}  // namespace pdf_to_md

#endif  // PDF_TO_MD_MARKDOWN_WRITER_H_
