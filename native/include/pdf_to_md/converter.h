#ifndef PDF_TO_MD_CONVERTER_H_
#define PDF_TO_MD_CONVERTER_H_

#include <atomic>
#include <memory>
#include <mutex>
#include <string>

#include "pdf_to_md/types.h"

namespace medical_ocr {
class PaddleOcrEngine;
}

namespace pdf_to_md {

class Converter {
public:
    Converter();
    ~Converter();

    bool Initialize(const std::string& models_dir_utf8,
                    const std::string& config_json_utf8);
    void Shutdown();

    int Convert(const std::string& pdf_path_utf8,
                const std::string& md_path_utf8,
                ProgressCallback progress);

    void Cancel();
    std::string GetLastError() const;

private:
    ConvertOptions options_;
    std::unique_ptr<medical_ocr::PaddleOcrEngine> engine_;
    std::atomic<bool> cancel_requested_{false};
    std::atomic<bool> busy_{false};
    mutable std::mutex error_mutex_;
    std::string last_error_;
    bool initialized_ = false;

    void SetError(const std::string& msg);
};

}  // namespace pdf_to_md

#endif  // PDF_TO_MD_CONVERTER_H_
