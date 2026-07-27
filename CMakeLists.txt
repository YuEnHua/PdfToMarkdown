# ============================================================================
# PdfToMarkdown — standalone CMake project
# ============================================================================
# Location: D:/work/AAA_21ic_Project/PdfToMarkdown
# Reuses OCR sources from sibling MedicalOCR via MEDICAL_OCR_ROOT.
#
# Configure:
#   cmake -S . -B build -G "Visual Studio 17 2022" -A x64 `
#     -DPADDLE_INFERENCE_DIR=D:/Environment/paddle_inference_3.3.0 `
#     -DOpenCV_DIR=D:/Environment/opencv/build
#   cmake --build build --config Release
# ============================================================================

cmake_minimum_required(VERSION 3.16)
project(PdfToMarkdown VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(PDF_TO_MD_STATIC_RUNTIME "Link MSVC runtime statically (/MT)" ON)
option(PDF_TO_MD_BUILD_TESTS "Build PdfToMarkdown unit tests" ON)
option(PDF_TO_MD_BUILD_CLI "Build PdfToMarkdown CLI" ON)
option(PDF_TO_MD_BUILD_GUI "Build Win32 desktop GUI" ON)

if(PDF_TO_MD_STATIC_RUNTIME AND MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# Sibling MedicalOCR project (OCR engine + paddle vendor + cmake modules)
set(MEDICAL_OCR_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../MedicalOCR"
    CACHE PATH "Path to MedicalOCR sibling project")
if(NOT EXISTS "${MEDICAL_OCR_ROOT}/include/medical_ocr/types.h")
    message(FATAL_ERROR
        "MedicalOCR not found at MEDICAL_OCR_ROOT=${MEDICAL_OCR_ROOT}. "
        "Expected sibling directory ../MedicalOCR")
endif()

list(APPEND CMAKE_MODULE_PATH "${MEDICAL_OCR_ROOT}/cmake")

# Reuse MedicalOCR dependency helpers (OpenCV / Paddle / nlohmann / optional gtest)
set(MEDICAL_OCR_USE_OPENCV ON)
set(MEDICAL_OCR_ENABLE_PADDLE ON)
set(MEDICAL_OCR_BUILD_TESTS ${PDF_TO_MD_BUILD_TESTS})
include("${MEDICAL_OCR_ROOT}/cmake/Dependencies.cmake")

set(PDF_TO_MD_VERSION "${PROJECT_VERSION}")
set(PDFIUM_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/pdfium")

if(NOT EXISTS "${PDFIUM_ROOT}/include/fpdfview.h")
    message(FATAL_ERROR "PDFium not found at ${PDFIUM_ROOT}. Run scripts/fetch_pdfium.ps1")
endif()

if(NOT TARGET nlohmann_json::nlohmann_json)
    message(FATAL_ERROR "nlohmann_json not configured")
endif()

if(NOT OpenCV_FOUND)
    message(FATAL_ERROR "OpenCV required. Set OpenCV_DIR (e.g. D:/Environment/opencv/build).")
endif()

if(NOT PADDLE_INFERENCE_FOUND)
    message(FATAL_ERROR
        "Paddle Inference required. Set PADDLE_INFERENCE_DIR "
        "(e.g. D:/Environment/paddle_inference_3.3.0).")
endif()

# ---- PDFium imported target ----
add_library(pdfium SHARED IMPORTED GLOBAL)
set_target_properties(pdfium PROPERTIES
    IMPORTED_LOCATION "${PDFIUM_ROOT}/bin/pdfium.dll"
    IMPORTED_IMPLIB "${PDFIUM_ROOT}/lib/pdfium.dll.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${PDFIUM_ROOT}/include"
)

# ---- Native sources ----
set(PDF_TO_MD_NATIVE_SOURCES
    native/src/pdf_renderer.cpp
    native/src/reading_order.cpp
    native/src/markdown_writer.cpp
    native/src/blank_line_detector.cpp
    native/src/converter.cpp
    native/src/pdf_to_md_c_api.cpp
    ${MEDICAL_OCR_ROOT}/src/ocr/paddle_ocr_engine.cpp
    ${MEDICAL_OCR_ROOT}/src/common/geometry_utils.cpp
    ${MEDICAL_OCR_ROOT}/src/common/utf8_utils.cpp
)

add_library(PdfToMarkdown.Native SHARED ${PDF_TO_MD_NATIVE_SOURCES})

target_include_directories(PdfToMarkdown.Native
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/native/include
        ${MEDICAL_OCR_ROOT}/include
    PRIVATE
        ${MEDICAL_OCR_ROOT}
        ${OpenCV_INCLUDE_DIRS}
)

target_compile_definitions(PdfToMarkdown.Native
    PRIVATE
        PDF_TO_MD_DLL_EXPORTS
        _CRT_SECURE_NO_WARNINGS
        NOMINMAX
)

if(MSVC)
    target_compile_options(PdfToMarkdown.Native PRIVATE /utf-8 /bigobj)
endif()

target_link_libraries(PdfToMarkdown.Native
    PRIVATE
        pdfium
        nlohmann_json::nlohmann_json
        ${OpenCV_LIBS}
)

include("${MEDICAL_OCR_ROOT}/cmake/PaddleOcrVendor.cmake")
medical_ocr_link_paddle_vendor(PdfToMarkdown.Native)
medical_ocr_copy_paddle_runtime(PdfToMarkdown.Native)

add_custom_command(TARGET PdfToMarkdown.Native POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${PDFIUM_ROOT}/bin/pdfium.dll"
        $<TARGET_FILE_DIR:PdfToMarkdown.Native>
    COMMENT "Copy pdfium.dll"
)

add_custom_command(TARGET PdfToMarkdown.Native POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:PdfToMarkdown.Native>/config"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/config/pdf_to_md.json"
        "$<TARGET_FILE_DIR:PdfToMarkdown.Native>/config/pdf_to_md.json"
)

set_target_properties(PdfToMarkdown.Native PROPERTIES
    OUTPUT_NAME PdfToMarkdown.Native
    PREFIX ""
)

# ---- CLI ----
if(PDF_TO_MD_BUILD_CLI)
    add_executable(PdfToMarkdown.Cli cli/main.cpp)
    target_link_libraries(PdfToMarkdown.Cli PRIVATE PdfToMarkdown.Native)
    target_include_directories(PdfToMarkdown.Cli PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/native/include)
    if(MSVC)
        target_compile_options(PdfToMarkdown.Cli PRIVATE /utf-8)
    endif()
    add_custom_command(TARGET PdfToMarkdown.Cli POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:PdfToMarkdown.Native>
            $<TARGET_FILE_DIR:PdfToMarkdown.Cli>
    )
endif()

# ---- Win32 GUI ----
if(PDF_TO_MD_BUILD_GUI AND WIN32)
    add_executable(PdfToMarkdown.Gui WIN32 app/win32_gui/main.cpp)
    target_link_libraries(PdfToMarkdown.Gui PRIVATE PdfToMarkdown.Native comctl32 shell32 ole32)
    target_include_directories(PdfToMarkdown.Gui PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/native/include)
    if(MSVC)
        target_compile_options(PdfToMarkdown.Gui PRIVATE /utf-8)
    endif()
    add_custom_command(TARGET PdfToMarkdown.Gui POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:PdfToMarkdown.Native>
            $<TARGET_FILE_DIR:PdfToMarkdown.Gui>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PDFIUM_ROOT}/bin/pdfium.dll"
            $<TARGET_FILE_DIR:PdfToMarkdown.Gui>
    )
endif()

# ---- Tests ----
if(PDF_TO_MD_BUILD_TESTS)
    if(NOT TARGET GTest::gtest)
        include(FetchContent)
        FetchContent_Declare(
            googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG v1.15.2
            GIT_SHALLOW TRUE
        )
        set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(googletest)
        enable_testing()
    endif()

    add_executable(PdfToMarkdown.Tests
        tests/test_reading_order.cpp
        tests/test_markdown_writer.cpp
        tests/test_error_codes.cpp
        tests/test_blank_line_detector.cpp
        native/src/reading_order.cpp
        native/src/markdown_writer.cpp
        native/src/blank_line_detector.cpp
        ${MEDICAL_OCR_ROOT}/src/common/geometry_utils.cpp
    )
    target_include_directories(PdfToMarkdown.Tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/native/include
        ${MEDICAL_OCR_ROOT}/include
        ${MEDICAL_OCR_ROOT}
        ${OpenCV_INCLUDE_DIRS}
    )
    target_link_libraries(PdfToMarkdown.Tests PRIVATE
        GTest::gtest
        GTest::gtest_main
        nlohmann_json::nlohmann_json
        ${OpenCV_LIBS}
    )
    if(MSVC)
        target_compile_options(PdfToMarkdown.Tests PRIVATE /utf-8)
        target_compile_definitions(PdfToMarkdown.Tests PRIVATE
            _CRT_SECURE_NO_WARNINGS NOMINMAX)
    endif()
    include(GoogleTest)
    gtest_discover_tests(PdfToMarkdown.Tests
        WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
    )
endif()

message(STATUS "PdfToMarkdown ${PDF_TO_MD_VERSION} configured (standalone)")
message(STATUS "  MedicalOCR: ${MEDICAL_OCR_ROOT}")
message(STATUS "  PDFium:     ${PDFIUM_ROOT}")
message(STATUS "  CLI:        ${PDF_TO_MD_BUILD_CLI}")
message(STATUS "  GUI:        ${PDF_TO_MD_BUILD_GUI}")
message(STATUS "  Tests:      ${PDF_TO_MD_BUILD_TESTS}")
