#!/usr/bin/env python3
"""Download Apple Silicon (arm64) prebuilt deps into third_party/.

Run from repo root or scripts/:  python scripts/fetch_macos_deps.py
Does not overwrite third_party/pdfium (Windows).
"""
from __future__ import annotations

import io
import os
import shutil
import sys
import tarfile
import zipfile
from pathlib import Path
from urllib.request import Request, urlopen

ROOT = Path(__file__).resolve().parents[1]
TP = ROOT / "third_party"
CACHE = TP / "_macos_download"
PDFIUM_DIR = TP / "pdfium-macos"
PADDLE_DIR = TP / "paddle_inference_macos"
OPENCV_DIR = TP / "opencv-macos"

PDFIUM_URL = (
    "https://github.com/bblanchon/pdfium-binaries/releases/download/"
    "chromium%2F7961/pdfium-mac-arm64.tgz"
)
PADDLE_URLS = [
    "https://paddle-inference-lib.bj.bcebos.com/3.2.1/cxx_c/MacOS/"
    "m1_clang_noavx_accelerate_blas/paddle_inference.tgz",
    "https://paddle-inference-lib.bj.bcebos.com/3.0.0/cxx_c/MacOS/"
    "m1_clang_noavx_accelerate_blas/paddle_inference.tgz",
]
CONDA_BASE = "https://conda.anaconda.org/conda-forge/osx-arm64"
OPENCV_CONDA = [
    "libopencv-4.13.0-headless_h293fd0e_16.conda",
    "libjpeg-turbo-3.2.0-h84a0fba_1.conda",
    "libpng-1.6.58-hf5e6511_1.conda",
    "libtiff-4.7.2-hf67920b_1.conda",
    "libwebp-base-1.6.0-h202fb40_1.conda",
    "libzlib-1.3.2-h8088a28_3.conda",
    "openjpeg-2.5.4-h4d1e80c_2.conda",
    "libblas-3.9.0-42_h09c003b_accelerate.conda",
    "libcblas-3.9.0-42_h752f6bc_accelerate.conda",
    "liblapack-3.9.0-42_hcb0d94e_accelerate.conda",
    "libavif16-1.4.2-hfc01230_4.conda",
    "libjxl-0.12.0-hb71b141_2.conda",
    "openexr-3.4.15-h3105456_0.conda",
    "imath-3.2.2-h3470cca_0.conda",
]


def log(msg: str) -> None:
    print(msg, flush=True)


def download(url: str, dest: Path) -> None:
    dest.parent.mkdir(parents=True, exist_ok=True)
    if dest.exists() and dest.stat().st_size > 1024:
        log(f"  cached {dest.name}")
        return
    log(f"  GET {url}")
    req = Request(url, headers={"User-Agent": "PdfToMarkdown-fetch/1.0"})
    with urlopen(req, timeout=180) as resp:
        data = resp.read()
    tmp = dest.with_suffix(dest.suffix + ".partial")
    tmp.write_bytes(data)
    tmp.replace(dest)
    log(f"  saved {dest.name} ({len(data)} bytes)")


def extract_tgz(archive: Path, dest: Path) -> None:
    dest.mkdir(parents=True, exist_ok=True)
    with tarfile.open(archive, "r:gz") as tar:
        tar.extractall(dest)


def flatten_if_single_dir(dest: Path) -> None:
    kids = [p for p in dest.iterdir() if p.name not in (".", "..")]
    if len(kids) == 1 and kids[0].is_dir():
        inner = kids[0]
        for item in inner.iterdir():
            target = dest / item.name
            if target.exists():
                if target.is_dir():
                    shutil.rmtree(target)
                else:
                    target.unlink()
            shutil.move(str(item), str(target))
        inner.rmdir()


def extract_conda(archive: Path, dest: Path) -> None:
    dest.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(archive) as zf:
        pkg = None
        for name in zf.namelist():
            if name.startswith("pkg-") and name.endswith(".tar.zst"):
                pkg = name
                break
        if not pkg:
            raise RuntimeError(f"No pkg-*.tar.zst in {archive.name}")
        raw = zf.read(pkg)

    try:
        import zstandard as zstd  # type: ignore
    except ImportError:
        import subprocess

        log("  installing zstandard (pip) to unpack .conda")
        subprocess.check_call(
            [sys.executable, "-m", "pip", "install", "--user", "zstandard"]
        )
        import zstandard as zstd  # type: ignore

    data = zstd.ZstdDecompressor().decompress(raw)
    with tarfile.open(fileobj=io.BytesIO(data), mode="r:") as tar:
        tar.extractall(dest)


def fetch_pdfium() -> None:
    log("[1/3] PDFium mac-arm64 (chromium/7961)")
    if (PDFIUM_DIR / "include" / "fpdfview.h").exists() and any(
        PDFIUM_DIR.joinpath("lib").glob("libpdfium*")
    ):
        log("  already present")
        return
    archive = CACHE / "pdfium-mac-arm64.tgz"
    download(PDFIUM_URL, archive)
    if PDFIUM_DIR.exists():
        shutil.rmtree(PDFIUM_DIR)
    extract_tgz(archive, PDFIUM_DIR)
    flatten_if_single_dir(PDFIUM_DIR)
    hdr = PDFIUM_DIR / "include" / "fpdfview.h"
    if not hdr.exists():
        raise RuntimeError(f"PDFium extract missing {hdr}")
    log(f"  ok {PDFIUM_DIR}")


def fetch_paddle() -> None:
    log("[2/3] Paddle Inference macOS m1")
    marker = PADDLE_DIR / "paddle" / "include" / "paddle_inference_api.h"
    if marker.exists():
        log("  already present")
        return
    last_err = None
    archive = CACHE / "paddle_inference_macos.tgz"
    for url in PADDLE_URLS:
        try:
            if archive.exists():
                archive.unlink()
            download(url, archive)
            last_err = None
            break
        except Exception as exc:  # noqa: BLE001
            last_err = exc
            log(f"  failed {url}: {exc}")
    if last_err and not archive.exists():
        raise RuntimeError(f"Paddle download failed: {last_err}")
    staging = CACHE / "paddle_extract"
    if staging.exists():
        shutil.rmtree(staging)
    extract_tgz(archive, staging)
    flatten_if_single_dir(staging)
    src = staging
    if not (src / "paddle" / "include" / "paddle_inference_api.h").exists():
        for child in src.iterdir():
            cand = child / "paddle" / "include" / "paddle_inference_api.h"
            if cand.exists():
                src = child
                break
    if PADDLE_DIR.exists():
        shutil.rmtree(PADDLE_DIR)
    shutil.copytree(src, PADDLE_DIR)
    if not marker.exists():
        raise RuntimeError("Paddle extract missing paddle_inference_api.h")
    log(f"  ok {PADDLE_DIR}")


def fetch_opencv() -> None:
    log("[3/3] OpenCV osx-arm64 (conda-forge libopencv + codecs/BLAS)")
    hdr = OPENCV_DIR / "include" / "opencv4" / "opencv2" / "core.hpp"
    OPENCV_DIR.mkdir(parents=True, exist_ok=True)
    errors = []
    for name in OPENCV_CONDA:
        url = f"{CONDA_BASE}/{name}"
        dest = CACHE / name
        try:
            download(url, dest)
            extract_conda(dest, OPENCV_DIR)
        except Exception as exc:  # noqa: BLE001
            errors.append(f"{name}: {exc}")
            log(f"  skip {name}: {exc}")
    if not hdr.exists() and not (OPENCV_DIR / "include" / "opencv2" / "core.hpp").exists():
        raise RuntimeError(
            "OpenCV extract missing headers. " + "; ".join(errors[:4])
        )
    log(f"  ok {OPENCV_DIR}")


def main() -> int:
    CACHE.mkdir(parents=True, exist_ok=True)
    fetch_pdfium()
    fetch_paddle()
    fetch_opencv()
    log("Done. Mac deps in third_party/{pdfium-macos,paddle_inference_macos,opencv-macos}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except KeyboardInterrupt:
        raise SystemExit(130)
    except Exception as exc:  # noqa: BLE001
        log(f"ERROR: {exc}")
        raise SystemExit(1)
