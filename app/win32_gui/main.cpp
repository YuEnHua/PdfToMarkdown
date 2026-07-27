/**
 * PdfToMarkdown Win32 GUI — Win11 x64 desktop UI
 *
 * Features: select PDF, choose output, DPI, start/cancel, progress, preview.
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
    std::wstring message;
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

std::wstring DefaultModelsPath() {
    wchar_t module[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, module, MAX_PATH);
    PathRemoveFileSpecW(module);
    std::wstring base = module;
    // Prefer ../models (MedicalOCR layout) then ./models then Environment
    std::wstring candidates[] = {
        base + L"\\models",
        base + L"\\..\\models",
        base + L"\\..\\..\\MedicalOCR\\models",
        L"D:\\Environment\\PaddleOCR-models\\PP-OCRv6",
        L"D:\\work\\AAA_21ic_Project\\MedicalOCR\\models",
    };
    for (const auto& c : candidates) {
        std::wstring det = c + L"\\PP-OCRv6_small_det\\inference.json";
        // Also accept when c itself is PP-OCRv6 root
        if (GetFileAttributesW(det.c_str()) != INVALID_FILE_ATTRIBUTES) {
            return c;
        }
        std::wstring det2 = c + L"\\inference.json";  // unlikely
        (void)det2;
    }
    // If models dir contains PP-OCRv6_small_* directly
    for (const auto& c : candidates) {
        std::wstring det = c + L"\\PP-OCRv6_small_det";
        if (GetFileAttributesW(det.c_str()) != INVALID_FILE_ATTRIBUTES) {
            return c;
        }
    }
    return base + L"\\models";
}

bool BrowseOpenPdf(HWND owner, std::wstring& path) {
    wchar_t file[MAX_PATH] = {};
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"PDF 文件 (*.pdf)\0*.pdf\0所有文件 (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER;
    if (GetOpenFileNameW(&ofn)) {
        path = file;
        return true;
    }
    return false;
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
    // Simple folder pick via GetOpenFileName with dummy filter is awkward;
    // use SHBrowseForFolder.
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

std::wstring SuggestMdPath(const std::wstring& pdf) {
    std::wstring out = pdf;
    size_t dot = out.find_last_of(L'.');
    if (dot != std::wstring::npos) out = out.substr(0, dot);
    out += L".md";
    return out;
}

void LoadPreview(const std::wstring& md_path) {
    std::ifstream ifs(md_path, std::ios::binary);
    if (!ifs) {
        SetWindowTextW(g.preview, L"（无法读取 Markdown 预览）");
        return;
    }
    std::string utf8((std::istreambuf_iterator<char>(ifs)),
                     std::istreambuf_iterator<char>());
    // Limit preview size
    if (utf8.size() > 200000) utf8.resize(200000);
    std::wstring wide = Utf8ToWide(utf8);
    SetWindowTextW(g.preview, wide.c_str());
}

void OnProgressCb(void* /*user*/, int current, int total, const char* message) {
    auto* msg = new ProgressMsg();
    msg->current = current;
    msg->total = total;
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

    std::wstring pdf = GetWindowTextStr(g.pdf_edit);
    std::wstring md = GetWindowTextStr(g.md_edit);
    std::wstring models = GetWindowTextStr(g.models_edit);
    std::wstring dpi_s = GetWindowTextStr(g.dpi_edit);
    int dpi = _wtoi(dpi_s.c_str());
    if (dpi < 150) dpi = 150;
    if (dpi > 300) dpi = 300;

    if (pdf.empty() || md.empty() || models.empty()) {
        MessageBoxW(g.hwnd, L"请填写 PDF、输出 Markdown 和模型目录。", L"提示",
                    MB_OK | MB_ICONWARNING);
        return;
    }

    char config[128];
    snprintf(config, sizeof(config),
             "{\"dpi\":%d,\"cpu_threads\":8,\"enable_mkldnn\":false}", dpi);

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
    SetUiRunning(true);
    SendMessageW(g.progress, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
    SendMessageW(g.progress, PBM_SETPOS, 0, 0);
    SetStatus(L"正在转换…");
    SetWindowTextW(g.preview, L"");

    std::string pdf_u8 = WideToUtf8(pdf);
    std::string md_u8 = WideToUtf8(md);

    if (g.worker.joinable()) g.worker.join();
    g.worker = std::thread([pdf_u8, md_u8]() {
        int rc = PdfToMd_Convert(g.handle, pdf_u8.c_str(), md_u8.c_str(),
                                 OnProgressCb, nullptr);
        std::string err = PdfToMd_GetLastError(g.handle)
                              ? PdfToMd_GetLastError(g.handle)
                              : "";
        auto* result = new std::pair<int, std::string>(rc, err);
        PostMessageW(g.hwnd, WM_APP_DONE, 0, reinterpret_cast<LPARAM>(result));
    });
}

void CancelConvert() {
    if (g.handle) PdfToMd_Cancel(g.handle);
    SetStatus(L"正在取消…");
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            g.hwnd = hwnd;
            CreateWindowW(L"STATIC", L"PDF 文件:", WS_CHILD | WS_VISIBLE, 16, 16,
                          80, 22, hwnd, nullptr, nullptr, nullptr);
            g.pdf_edit = CreateWindowExW(
                WS_EX_CLIENTEDGE, L"EDIT", L"",
                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 100, 12, 520, 26, hwnd,
                (HMENU)IDC_PDF_EDIT, nullptr, nullptr);
            CreateWindowW(L"BUTTON", L"浏览…", WS_CHILD | WS_VISIBLE, 630, 12, 80,
                          26, hwnd, (HMENU)IDC_BROWSE_PDF, nullptr, nullptr);

            CreateWindowW(L"STATIC", L"输出 MD:", WS_CHILD | WS_VISIBLE, 16, 50,
                          80, 22, hwnd, nullptr, nullptr, nullptr);
            g.md_edit = CreateWindowExW(
                WS_EX_CLIENTEDGE, L"EDIT", L"",
                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 100, 46, 520, 26, hwnd,
                (HMENU)IDC_MD_EDIT, nullptr, nullptr);
            CreateWindowW(L"BUTTON", L"浏览…", WS_CHILD | WS_VISIBLE, 630, 46, 80,
                          26, hwnd, (HMENU)IDC_BROWSE_MD, nullptr, nullptr);

            CreateWindowW(L"STATIC", L"模型目录:", WS_CHILD | WS_VISIBLE, 16, 84,
                          80, 22, hwnd, nullptr, nullptr, nullptr);
            g.models_edit = CreateWindowExW(
                WS_EX_CLIENTEDGE, L"EDIT", DefaultModelsPath().c_str(),
                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 100, 80, 520, 26, hwnd,
                (HMENU)IDC_MODELS_EDIT, nullptr, nullptr);
            CreateWindowW(L"BUTTON", L"浏览…", WS_CHILD | WS_VISIBLE, 630, 80, 80,
                          26, hwnd, (HMENU)IDC_BROWSE_MODELS, nullptr, nullptr);

            CreateWindowW(L"STATIC", L"DPI:", WS_CHILD | WS_VISIBLE, 16, 118, 80,
                          22, hwnd, nullptr, nullptr, nullptr);
            g.dpi_edit = CreateWindowExW(
                WS_EX_CLIENTEDGE, L"EDIT", L"200",
                WS_CHILD | WS_VISIBLE | ES_NUMBER, 100, 114, 80, 26, hwnd,
                (HMENU)IDC_DPI_EDIT, nullptr, nullptr);

            g.start_btn =
                CreateWindowW(L"BUTTON", L"开始转换", WS_CHILD | WS_VISIBLE, 200,
                              114, 100, 28, hwnd, (HMENU)IDC_START, nullptr,
                              nullptr);
            g.cancel_btn =
                CreateWindowW(L"BUTTON", L"取消", WS_CHILD | WS_VISIBLE, 310,
                              114, 80, 28, hwnd, (HMENU)IDC_CANCEL, nullptr,
                              nullptr);
            EnableWindow(g.cancel_btn, FALSE);
            CreateWindowW(L"BUTTON", L"打开 MD", WS_CHILD | WS_VISIBLE, 400, 114,
                          90, 28, hwnd, (HMENU)IDC_OPEN_MD, nullptr, nullptr);
            CreateWindowW(L"BUTTON", L"打开目录", WS_CHILD | WS_VISIBLE, 500, 114,
                          90, 28, hwnd, (HMENU)IDC_OPEN_FOLDER, nullptr, nullptr);

            g.progress = CreateWindowExW(
                0, PROGRESS_CLASSW, nullptr, WS_CHILD | WS_VISIBLE, 16, 156, 694,
                22, hwnd, (HMENU)IDC_PROGRESS, nullptr, nullptr);
            g.status = CreateWindowW(L"STATIC", L"就绪。拖入或选择 PDF 后点击开始。",
                                     WS_CHILD | WS_VISIBLE, 16, 186, 694, 22,
                                     hwnd, (HMENU)IDC_STATUS, nullptr, nullptr);

            CreateWindowW(L"STATIC", L"Markdown 预览:", WS_CHILD | WS_VISIBLE, 16,
                          218, 140, 22, hwnd, nullptr, nullptr, nullptr);
            g.preview = CreateWindowExW(
                WS_EX_CLIENTEDGE, L"EDIT", L"",
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE |
                    ES_AUTOVSCROLL | ES_READONLY,
                16, 242, 694, 300, hwnd, (HMENU)IDC_PREVIEW, nullptr, nullptr);

            DragAcceptFiles(hwnd, TRUE);
            return 0;
        }
        case WM_DROPFILES: {
            HDROP drop = (HDROP)wParam;
            wchar_t file[MAX_PATH] = {};
            if (DragQueryFileW(drop, 0, file, MAX_PATH)) {
                std::wstring path = file;
                if (path.size() > 4) {
                    std::wstring ext = path.substr(path.size() - 4);
                    for (auto& c : ext) c = towlower(c);
                    if (ext == L".pdf") {
                        SetWindowTextW(g.pdf_edit, path.c_str());
                        SetWindowTextW(g.md_edit, SuggestMdPath(path).c_str());
                        SetStatus(L"已载入 PDF，可开始转换。");
                    }
                }
            }
            DragFinish(drop);
            return 0;
        }
        case WM_COMMAND: {
            const int id = LOWORD(wParam);
            if (id == IDC_BROWSE_PDF) {
                std::wstring path;
                if (BrowseOpenPdf(hwnd, path)) {
                    SetWindowTextW(g.pdf_edit, path.c_str());
                    SetWindowTextW(g.md_edit, SuggestMdPath(path).c_str());
                }
            } else if (id == IDC_BROWSE_MD) {
                std::wstring path = GetWindowTextStr(g.md_edit);
                if (BrowseSaveMd(hwnd, path)) {
                    SetWindowTextW(g.md_edit, path.c_str());
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
                if (!md.empty()) {
                    ShellExecuteW(hwnd, L"open", md.c_str(), nullptr, nullptr,
                                  SW_SHOWNORMAL);
                }
            } else if (id == IDC_OPEN_FOLDER) {
                std::wstring md = GetWindowTextStr(g.md_edit);
                if (!md.empty()) {
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
                if (msg->total > 0) {
                    pct = static_cast<int>((msg->current * 100.0) / msg->total);
                }
                SendMessageW(g.progress, PBM_SETPOS, pct, 0);
                std::wstring s = L"[" + std::to_wstring(msg->current) + L"/" +
                                 std::to_wstring(msg->total) + L"] " +
                                 msg->message;
                SetStatus(s);
                delete msg;
            }
            return 0;
        }
        case WM_APP_DONE: {
            auto* result =
                reinterpret_cast<std::pair<int, std::string>*>(lParam);
            g.running = false;
            SetUiRunning(false);
            if (g.worker.joinable()) g.worker.join();

            if (result) {
                if (result->first == PDFMD_OK) {
                    SetStatus(L"转换完成。");
                    SendMessageW(g.progress, PBM_SETPOS, 100, 0);
                    LoadPreview(GetWindowTextStr(g.md_edit));
                    MessageBoxW(hwnd, L"PDF 已成功转换为 Markdown。", L"完成",
                                MB_OK | MB_ICONINFORMATION);
                } else if (result->first == PDFMD_ERR_CANCELLED) {
                    SetStatus(L"已取消。");
                } else {
                    std::wstring err = Utf8ToWide(result->second);
                    SetStatus(L"失败: " + err);
                    MessageBoxW(hwnd, err.c_str(), L"转换失败",
                                MB_OK | MB_ICONERROR);
                }
                delete result;
            }
            return 0;
        }
        case WM_DESTROY:
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
