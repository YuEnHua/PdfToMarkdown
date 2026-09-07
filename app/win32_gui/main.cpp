/**
 * PdfToMarkdown Win32 GUI — Win11 x64 desktop UI
 *
 * Features: multi PDF select/drop, dual .md+.txt beside each PDF,
 * DPI, start/cancel, progress, preview.
 * Uses PdfToMarkdown.Native.dll via C ABI.
 */

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <shlobj.h>

#include <atomic>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "pdf_to_md/pdf_to_md_c_api.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")

namespace {

constexpr int IDC_PDF_EDIT = 1001;
constexpr int IDC_MD_EDIT = 1002;
constexpr int IDC_MODELS_EDIT = 1003;
constexpr int IDC_DPI_EDIT = 1004;
constexpr int IDC_BROWSE_PDF = 1005;
constexpr int IDC_BROWSE_MD = 1006;
constexpr int IDC_BROWSE_MODELS = 1007;
constexpr int IDC_START = 1008;
constexpr int IDC_CANCEL = 1009;
constexpr int IDC_OPEN_MD = 1010;
constexpr int IDC_OPEN_FOLDER = 1011;
constexpr int IDC_PROGRESS = 1012;
constexpr int IDC_STATUS = 1013;
constexpr int IDC_PREVIEW = 1014;

constexpr UINT WM_APP_PROGRESS = WM_APP + 1;
constexpr UINT WM_APP_DONE = WM_APP + 2;

struct ProgressMsg {
    int current = 0;
    int total = 0;
    int file_index = 0;  // 1-based; 0 = unknown
    int file_count = 0;
    std::wstring message;
};

struct BatchResult {
    int ok = 0;
    int failed = 0;
    int cancelled = 0;
    std::string last_error;
    std::wstring last_md_path;
};

struct BatchProgressCtx {
    int file_index = 0;
    int file_count = 0;
};

struct AppState {
    HWND hwnd = nullptr;
    HWND pdf_edit = nullptr;
    HWND md_edit = nullptr;
    HWND models_edit = nullptr;
    HWND dpi_edit = nullptr;
    HWND progress = nullptr;
    HWND status = nullptr;
    HWND preview = nullptr;
    HWND start_btn = nullptr;
    HWND cancel_btn = nullptr;

    PdfToMdHandle handle = nullptr;
    std::atomic<bool> running{false};
    std::atomic<bool> cancel_batch{false};
    std::thread worker;
};

AppState g;

std::wstring Utf8ToWide(const std::string& utf8) {
    if (utf8.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    if (n <= 0) return {};
    std::wstring w(static_cast<size_t>(n - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &w[0], n);
    return w;
}

std::string WideToUtf8(const std::wstring& wide) {
    if (wide.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0, nullptr,
                                nullptr);
    if (n <= 0) return {};
    std::string s(static_cast<size_t>(n - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, &s[0], n, nullptr, nullptr);
    return s;
}

std::wstring GetWindowTextStr(HWND h) {
    int len = GetWindowTextLengthW(h);
    std::wstring s(static_cast<size_t>(len), L'\0');
    if (len > 0) GetWindowTextW(h, &s[0], len + 1);
    return s;
}

void SetStatus(const std::wstring& text) {
    if (g.status) SetWindowTextW(g.status, text.c_str());
}

bool EndsWithPdf(const std::wstring& path) {
    if (path.size() < 4) return false;
    std::wstring ext = path.substr(path.size() - 4);
    for (auto& c : ext) c = towlower(c);
    return ext == L".pdf";
}

std::wstring SuggestMdPath(const std::wstring& pdf) {
    std::wstring out = pdf;
    size_t dot = out.find_last_of(L'.');
    if (dot != std::wstring::npos) out = out.substr(0, dot);
    out += L".md";
    return out;
}

std::wstring JoinPathsMultiline(const std::vector<std::wstring>& paths) {
    std::wstring out;
    for (size_t i = 0; i < paths.size(); ++i) {
        if (i) out += L"\r\n";
        out += paths[i];
    }
    return out;
}

std::vector<std::wstring> ParsePdfList(const std::wstring& text) {
    std::vector<std::wstring> out;
    std::wstring cur;
    for (size_t i = 0; i < text.size(); ++i) {
        const wchar_t c = text[i];
        if (c == L'\r' || c == L'\n' || c == L';') {
            if (!cur.empty()) {
                if (EndsWithPdf(cur)) out.push_back(cur);
                cur.clear();
            }
            if (c == L'\r' && i + 1 < text.size() && text[i + 1] == L'\n') ++i;
        } else {
            cur.push_back(c);
        }
    }
    if (!cur.empty() && EndsWithPdf(cur)) out.push_back(cur);
    return out;
}

void UpdateMdHintFromPdfs(const std::vector<std::wstring>& pdfs) {
    if (pdfs.empty()) {
        SetWindowTextW(g.md_edit, L"");
        return;
    }
    if (pdfs.size() == 1) {
        SetWindowTextW(g.md_edit, SuggestMdPath(pdfs[0]).c_str());
        return;
    }
    SetWindowTextW(g.md_edit,
                   L"（批量：各 PDF 同目录同名 .md / .txt）");
}

std::wstring DefaultModelsPath() {
    wchar_t module[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, module, MAX_PATH);
    PathRemoveFileSpecW(module);
    std::wstring base = module;
    std::wstring candidates[] = {
        base + L"\\models",
        base + L"\\..\\models",
        base + L"\\..\\..\\MedicalOCR\\models",
        L"D:\\Environment\\PaddleOCR-models\\PP-OCRv6",
        L"D:\\work\\AAA_21ic_Project\\MedicalOCR\\models",
    };
    for (const auto& c : candidates) {
        std::wstring det = c + L"\\PP-OCRv6_small_det\\inference.json";
        if (GetFileAttributesW(det.c_str()) != INVALID_FILE_ATTRIBUTES) {
            return c;
        }
    }
    for (const auto& c : candidates) {
        std::wstring det = c + L"\\PP-OCRv6_small_det";
        if (GetFileAttributesW(det.c_str()) != INVALID_FILE_ATTRIBUTES) {
            return c;
        }
    }
    return base + L"\\models";
}

bool BrowseOpenPdfs(HWND owner, std::vector<std::wstring>& paths) {
    // OFN_ALLOWMULTISELECT needs a large buffer: dir\0file1\0file2\0\0
    constexpr DWORD kBufChars = 64 * 1024;
    std::vector<wchar_t> file(kBufChars, L'\0');
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFile = file.data();
    ofn.nMaxFile = kBufChars;
    ofn.lpstrFilter = L"PDF 文件 (*.pdf)\0*.pdf\0所有文件 (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER |
                OFN_ALLOWMULTISELECT;
    if (!GetOpenFileNameW(&ofn)) return false;

    paths.clear();
    const wchar_t* p = file.data();
    std::wstring first = p;
    p += first.size() + 1;
    if (*p == L'\0') {
        // Single selection
        if (EndsWithPdf(first)) paths.push_back(first);
    } else {
        std::wstring dir = first;
        if (!dir.empty() && dir.back() != L'\\' && dir.back() != L'/') {
            dir += L'\\';
        }
        while (*p) {
            std::wstring name = p;
            p += name.size() + 1;
            std::wstring full = dir + name;
            if (EndsWithPdf(full)) paths.push_back(full);
        }
    }
    return !paths.empty();
}

bool BrowseSaveMd(HWND owner, std::wstring& path) {
    wchar_t file[MAX_PATH] = {};
    if (!path.empty() && path.size() < MAX_PATH) {
        wcsncpy_s(file, path.c_str(), _TRUNCATE);
    }
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"Markdown (*.md)\0*.md\0所有文件 (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrDefExt = L"md";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_EXPLORER;
    if (GetSaveFileNameW(&ofn)) {
        path = file;
        return true;
    }
    return false;
}

bool BrowseFolder(HWND owner, std::wstring& path) {
    BROWSEINFOW bi = {};
    bi.hwndOwner = owner;
    bi.lpszTitle = L"选择 PP-OCRv6 模型目录（含 PP-OCRv6_small_det/rec）";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
    if (!pidl) return false;
    wchar_t folder[MAX_PATH] = {};
    BOOL ok = SHGetPathFromIDListW(pidl, folder);
    CoTaskMemFree(pidl);
    if (!ok) return false;
    path = folder;
    return true;
}

void LoadPreview(const std::wstring& md_path) {
    std::ifstream ifs(md_path, std::ios::binary);
    if (!ifs) {
        SetWindowTextW(g.preview, L"（无法读取 Markdown 预览）");
        return;
    }
    std::string utf8((std::istreambuf_iterator<char>(ifs)),
                     std::istreambuf_iterator<char>());
    if (utf8.size() > 200000) utf8.resize(200000);
    std::wstring wide = Utf8ToWide(utf8);
    SetWindowTextW(g.preview, wide.c_str());
}

void OnProgressCb(void* user, int current, int total, const char* message) {
    auto* ctx = static_cast<BatchProgressCtx*>(user);
    auto* msg = new ProgressMsg();
    msg->current = current;
    msg->total = total;
    if (ctx) {
        msg->file_index = ctx->file_index;
        msg->file_count = ctx->file_count;
    }
    msg->message = Utf8ToWide(message ? message : "");
    PostMessageW(g.hwnd, WM_APP_PROGRESS, 0, reinterpret_cast<LPARAM>(msg));
}

void SetUiRunning(bool running) {
    EnableWindow(g.start_btn, running ? FALSE : TRUE);
    EnableWindow(g.cancel_btn, running ? TRUE : FALSE);
    EnableWindow(g.pdf_edit, running ? FALSE : TRUE);
    EnableWindow(g.md_edit, running ? FALSE : TRUE);
}

void StartConvert() {
    if (g.running) return;

    auto pdfs = ParsePdfList(GetWindowTextStr(g.pdf_edit));
    std::wstring md_hint = GetWindowTextStr(g.md_edit);
    std::wstring models = GetWindowTextStr(g.models_edit);
    std::wstring dpi_s = GetWindowTextStr(g.dpi_edit);
    int dpi = _wtoi(dpi_s.c_str());
    if (dpi < 150) dpi = 150;
    if (dpi > 300) dpi = 300;

    if (pdfs.empty() || models.empty()) {
        MessageBoxW(g.hwnd, L"请填写至少一个 PDF 和模型目录。", L"提示",
                    MB_OK | MB_ICONWARNING);
        return;
    }

    std::vector<std::pair<std::wstring, std::wstring>> jobs;
    jobs.reserve(pdfs.size());
    if (pdfs.size() == 1) {
        std::wstring md = md_hint;
        if (md.empty() || md.find(L"（批量") != std::wstring::npos) {
            md = SuggestMdPath(pdfs[0]);
        }
        jobs.emplace_back(pdfs[0], md);
    } else {
        for (const auto& pdf : pdfs) {
            jobs.emplace_back(pdf, SuggestMdPath(pdf));
        }
    }

    char config[160];
    snprintf(config, sizeof(config),
             "{\"dpi\":%d,\"cpu_threads\":8,\"enable_mkldnn\":false,"
             "\"flush_each_page\":true}",
             dpi);

    if (g.handle) {
        PdfToMd_Destroy(g.handle);
        g.handle = nullptr;
    }

    std::string models_u8 = WideToUtf8(models);
    int rc = PdfToMd_Create(models_u8.c_str(), config, &g.handle);
    if (rc != PDFMD_OK) {
        std::wstring err = Utf8ToWide(PdfToMd_GetLastError(g.handle));
        MessageBoxW(g.hwnd, err.c_str(), L"初始化失败", MB_OK | MB_ICONERROR);
        return;
    }

    g.running = true;
    g.cancel_batch = false;
    SetUiRunning(true);
    SendMessageW(g.progress, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
    SendMessageW(g.progress, PBM_SETPOS, 0, 0);
    SetStatus(L"正在转换…");
    SetWindowTextW(g.preview, L"");

    if (g.worker.joinable()) g.worker.join();
    g.worker = std::thread([jobs]() {
        BatchResult* result = new BatchResult();
        const int n = static_cast<int>(jobs.size());
        BatchProgressCtx ctx;
        ctx.file_count = n;

        for (int i = 0; i < n; ++i) {
            if (g.cancel_batch.load()) {
                result->cancelled += (n - i);
                break;
            }
            ctx.file_index = i + 1;
            const std::string pdf_u8 = WideToUtf8(jobs[i].first);
            const std::string md_u8 = WideToUtf8(jobs[i].second);
            int rc = PdfToMd_Convert(g.handle, pdf_u8.c_str(), md_u8.c_str(),
                                     OnProgressCb, &ctx);
            if (rc == PDFMD_OK) {
                ++result->ok;
                result->last_md_path = jobs[i].second;
            } else if (rc == PDFMD_ERR_CANCELLED) {
                ++result->cancelled;
                result->cancelled += (n - i - 1);
                result->last_error = PdfToMd_GetLastError(g.handle)
                                         ? PdfToMd_GetLastError(g.handle)
                                         : "Cancelled";
                break;
            } else {
                ++result->failed;
                result->last_error = PdfToMd_GetLastError(g.handle)
                                         ? PdfToMd_GetLastError(g.handle)
                                         : "Convert failed";
                // Continue remaining files on non-cancel errors.
            }
        }

        PostMessageW(g.hwnd, WM_APP_DONE, 0, reinterpret_cast<LPARAM>(result));
    });
}

void CancelConvert() {
    g.cancel_batch = true;
    if (g.handle) PdfToMd_Cancel(g.handle);
    SetStatus(L"正在取消…");
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            g.hwnd = hwnd;
            CreateWindowW(L"STATIC", L"PDF 列表:", WS_CHILD | WS_VISIBLE, 16, 16,
                          80, 22, hwnd, nullptr, nullptr, nullptr);
            g.pdf_edit = CreateWindowExW(
                WS_EX_CLIENTEDGE, L"EDIT", L"",
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE |
                    ES_AUTOVSCROLL | ES_WANTRETURN,
                100, 12, 520, 64, hwnd, (HMENU)IDC_PDF_EDIT, nullptr, nullptr);
            CreateWindowW(L"BUTTON", L"浏览…", WS_CHILD | WS_VISIBLE, 630, 12, 80,
                          26, hwnd, (HMENU)IDC_BROWSE_PDF, nullptr, nullptr);

            CreateWindowW(L"STATIC", L"输出 MD:", WS_CHILD | WS_VISIBLE, 16, 86,
                          80, 22, hwnd, nullptr, nullptr, nullptr);
            g.md_edit = CreateWindowExW(
                WS_EX_CLIENTEDGE, L"EDIT", L"",
                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 100, 82, 520, 26, hwnd,
                (HMENU)IDC_MD_EDIT, nullptr, nullptr);
            CreateWindowW(L"BUTTON", L"浏览…", WS_CHILD | WS_VISIBLE, 630, 82, 80,
                          26, hwnd, (HMENU)IDC_BROWSE_MD, nullptr, nullptr);

            CreateWindowW(L"STATIC", L"模型目录:", WS_CHILD | WS_VISIBLE, 16, 120,
                          80, 22, hwnd, nullptr, nullptr, nullptr);
            g.models_edit = CreateWindowExW(
                WS_EX_CLIENTEDGE, L"EDIT", DefaultModelsPath().c_str(),
                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 100, 116, 520, 26, hwnd,
                (HMENU)IDC_MODELS_EDIT, nullptr, nullptr);
            CreateWindowW(L"BUTTON", L"浏览…", WS_CHILD | WS_VISIBLE, 630, 116,
                          80, 26, hwnd, (HMENU)IDC_BROWSE_MODELS, nullptr,
                          nullptr);

            CreateWindowW(L"STATIC", L"DPI:", WS_CHILD | WS_VISIBLE, 16, 154, 80,
                          22, hwnd, nullptr, nullptr, nullptr);
            g.dpi_edit = CreateWindowExW(
                WS_EX_CLIENTEDGE, L"EDIT", L"200",
                WS_CHILD | WS_VISIBLE | ES_NUMBER, 100, 150, 80, 26, hwnd,
                (HMENU)IDC_DPI_EDIT, nullptr, nullptr);

            g.start_btn =
                CreateWindowW(L"BUTTON", L"开始转换", WS_CHILD | WS_VISIBLE, 200,
                              150, 100, 28, hwnd, (HMENU)IDC_START, nullptr,
                              nullptr);
            g.cancel_btn =
                CreateWindowW(L"BUTTON", L"取消", WS_CHILD | WS_VISIBLE, 310,
                              150, 80, 28, hwnd, (HMENU)IDC_CANCEL, nullptr,
                              nullptr);
            EnableWindow(g.cancel_btn, FALSE);
            CreateWindowW(L"BUTTON", L"打开 MD", WS_CHILD | WS_VISIBLE, 400, 150,
                          90, 28, hwnd, (HMENU)IDC_OPEN_MD, nullptr, nullptr);
            CreateWindowW(L"BUTTON", L"打开目录", WS_CHILD | WS_VISIBLE, 500, 150,
                          90, 28, hwnd, (HMENU)IDC_OPEN_FOLDER, nullptr,
                          nullptr);

            g.progress = CreateWindowExW(
                0, PROGRESS_CLASSW, nullptr, WS_CHILD | WS_VISIBLE, 16, 192, 694,
                22, hwnd, (HMENU)IDC_PROGRESS, nullptr, nullptr);
            g.status = CreateWindowW(
                L"STATIC",
                L"就绪。可多选/拖入多个 PDF；输出为同名 .md 与 .txt。",
                WS_CHILD | WS_VISIBLE, 16, 222, 694, 22, hwnd,
                (HMENU)IDC_STATUS, nullptr, nullptr);

            CreateWindowW(L"STATIC", L"Markdown 预览:", WS_CHILD | WS_VISIBLE, 16,
                          254, 140, 22, hwnd, nullptr, nullptr, nullptr);
            g.preview = CreateWindowExW(
                WS_EX_CLIENTEDGE, L"EDIT", L"",
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE |
                    ES_AUTOVSCROLL | ES_READONLY,
                16, 278, 694, 270, hwnd, (HMENU)IDC_PREVIEW, nullptr, nullptr);

            DragAcceptFiles(hwnd, TRUE);
            return 0;
        }
        case WM_DROPFILES: {
            HDROP drop = (HDROP)wParam;
            const UINT count = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
            std::vector<std::wstring> pdfs;
            for (UINT i = 0; i < count; ++i) {
                wchar_t file[MAX_PATH] = {};
                if (DragQueryFileW(drop, i, file, MAX_PATH) &&
                    EndsWithPdf(file)) {
                    pdfs.push_back(file);
                }
            }
            DragFinish(drop);
            if (!pdfs.empty()) {
                SetWindowTextW(g.pdf_edit, JoinPathsMultiline(pdfs).c_str());
                UpdateMdHintFromPdfs(pdfs);
                SetStatus(L"已载入 " + std::to_wstring(pdfs.size()) +
                          L" 个 PDF，可开始转换。");
            }
            return 0;
        }
        case WM_COMMAND: {
            const int id = LOWORD(wParam);
            if (id == IDC_BROWSE_PDF) {
                std::vector<std::wstring> paths;
                if (BrowseOpenPdfs(hwnd, paths)) {
                    SetWindowTextW(g.pdf_edit,
                                   JoinPathsMultiline(paths).c_str());
                    UpdateMdHintFromPdfs(paths);
                }
            } else if (id == IDC_BROWSE_MD) {
                auto pdfs = ParsePdfList(GetWindowTextStr(g.pdf_edit));
                if (pdfs.size() > 1) {
                    MessageBoxW(
                        hwnd,
                        L"批量模式下输出固定为各 PDF 旁同名 .md / .txt，"
                        L"无需单独选择。",
                        L"提示", MB_OK | MB_ICONINFORMATION);
                } else {
                    std::wstring path = GetWindowTextStr(g.md_edit);
                    if (path.find(L"（批量") != std::wstring::npos) path.clear();
                    if (BrowseSaveMd(hwnd, path)) {
                        SetWindowTextW(g.md_edit, path.c_str());
                    }
                }
            } else if (id == IDC_BROWSE_MODELS) {
                std::wstring path;
                if (BrowseFolder(hwnd, path)) {
                    SetWindowTextW(g.models_edit, path.c_str());
                }
            } else if (id == IDC_START) {
                StartConvert();
            } else if (id == IDC_CANCEL) {
                CancelConvert();
            } else if (id == IDC_OPEN_MD) {
                std::wstring md = GetWindowTextStr(g.md_edit);
                if (md.find(L"（批量") != std::wstring::npos) {
                    auto pdfs = ParsePdfList(GetWindowTextStr(g.pdf_edit));
                    if (!pdfs.empty()) md = SuggestMdPath(pdfs[0]);
                }
                if (!md.empty() && md.find(L"（批量") == std::wstring::npos) {
                    ShellExecuteW(hwnd, L"open", md.c_str(), nullptr, nullptr,
                                  SW_SHOWNORMAL);
                }
            } else if (id == IDC_OPEN_FOLDER) {
                std::wstring md = GetWindowTextStr(g.md_edit);
                if (md.find(L"（批量") != std::wstring::npos) {
                    auto pdfs = ParsePdfList(GetWindowTextStr(g.pdf_edit));
                    if (!pdfs.empty()) md = SuggestMdPath(pdfs[0]);
                }
                if (!md.empty() && md.find(L"（批量") == std::wstring::npos) {
                    std::wstring arg = L"/select,\"" + md + L"\"";
                    ShellExecuteW(hwnd, L"open", L"explorer.exe", arg.c_str(),
                                  nullptr, SW_SHOWNORMAL);
                }
            }
            return 0;
        }
        case WM_APP_PROGRESS: {
            auto* msg = reinterpret_cast<ProgressMsg*>(lParam);
            if (msg) {
                int pct = 0;
                if (msg->file_count > 0 && msg->total > 0) {
                    // Overall: completed files + current file page fraction.
                    const double file_frac =
                        (static_cast<double>(msg->file_index - 1) +
                         (static_cast<double>(msg->current) /
                          static_cast<double>(msg->total))) /
                        static_cast<double>(msg->file_count);
                    pct = static_cast<int>(file_frac * 100.0);
                } else if (msg->total > 0) {
                    pct = static_cast<int>((msg->current * 100.0) / msg->total);
                }
                if (pct < 0) pct = 0;
                if (pct > 100) pct = 100;
                SendMessageW(g.progress, PBM_SETPOS, pct, 0);
                std::wstring s;
                if (msg->file_count > 0) {
                    s += L"文件 " + std::to_wstring(msg->file_index) + L"/" +
                         std::to_wstring(msg->file_count) + L"  ";
                }
                s += L"页 [" + std::to_wstring(msg->current) + L"/" +
                     std::to_wstring(msg->total) + L"] " + msg->message;
                SetStatus(s);
                delete msg;
            }
            return 0;
        }
        case WM_APP_DONE: {
            auto* result = reinterpret_cast<BatchResult*>(lParam);
            g.running = false;
            SetUiRunning(false);
            if (g.worker.joinable()) g.worker.join();

            if (result) {
                const int total =
                    result->ok + result->failed + result->cancelled;
                std::wstring summary =
                    L"完成：成功 " + std::to_wstring(result->ok) + L"，失败 " +
                    std::to_wstring(result->failed) + L"，取消 " +
                    std::to_wstring(result->cancelled) + L"（共 " +
                    std::to_wstring(total) + L"）";
                SetStatus(summary);
                if (result->ok > 0) {
                    SendMessageW(g.progress, PBM_SETPOS, 100, 0);
                    if (!result->last_md_path.empty()) {
                        LoadPreview(result->last_md_path);
                    }
                }
                if (result->failed > 0 && result->ok == 0 &&
                    result->cancelled == 0) {
                    MessageBoxW(hwnd, Utf8ToWide(result->last_error).c_str(),
                                L"转换失败", MB_OK | MB_ICONERROR);
                } else if (result->cancelled > 0 && result->ok == 0) {
                    // status already set
                } else {
                    MessageBoxW(hwnd, summary.c_str(), L"批量转换",
                                MB_OK | MB_ICONINFORMATION);
                }
                delete result;
            }
            return 0;
        }
        case WM_DESTROY:
            g.cancel_batch = true;
            if (g.running && g.handle) PdfToMd_Cancel(g.handle);
            if (g.worker.joinable()) g.worker.join();
            if (g.handle) {
                PdfToMd_Destroy(g.handle);
                g.handle = nullptr;
            }
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

}  // namespace

int WINAPI wWinMain(HINSTANCE hi, HINSTANCE, PWSTR, int show) {
    InitCommonControls();
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hi;
    wc.lpszClassName = L"PdfToMarkdownGuiClass";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassW(&wc);

    HWND hwnd =
        CreateWindowExW(0, wc.lpszClassName, L"PDF OCR → Markdown（Win11）",
                        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                        CW_USEDEFAULT, CW_USEDEFAULT, 740, 620, nullptr, nullptr,
                        hi, nullptr);
    ShowWindow(hwnd, show);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    CoUninitialize();
    return static_cast<int>(msg.wParam);
}
