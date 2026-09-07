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
 * Plain-text export (no Markdown escaping). Page markers as
 * "----- page N -----"; paragraphs separated by blank lines.
 */
std::string BuildPlainTextDocument(
    const std::string& source_pdf_name,
    const std::vector<PageMarkdown>& pages);

/** Derive sibling .txt path from a .md (or any) output path. */
std::string DeriveTxtPathFromMdPath(const std::string& md_path_utf8);

/**
 * Write UTF-8 text (no BOM) via temp file then atomic replace.
 * Used for both .md and .txt.
 */
bool WriteTextAtomic(
    const std::string& path_utf8,
    const std::string& content,
    std::string& error_out);

/** Alias kept for callers/tests; same as WriteTextAtomic. */
bool WriteMarkdownAtomic(
    const std::string& md_path_utf8,
    const std::string& content,
    std::string& error_out);

}  // namespace pdf_to_md

#endif  // PDF_TO_MD_MARKDOWN_WRITER_H_
