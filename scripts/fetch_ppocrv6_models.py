#!/usr/bin/env python3
"""Download PP-OCRv6 small det/rec inference files (Hugging Face)."""
from __future__ import annotations

import sys
from pathlib import Path
from urllib.request import Request, urlopen

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "models"
HF = "https://huggingface.co/PaddlePaddle/{repo}/resolve/main/{file}"
REPOS = {
    "PP-OCRv6_small_det": "PP-OCRv6_small_det",
    "PP-OCRv6_small_rec": "PP-OCRv6_small_rec",
}
FILES = ("inference.json", "inference.pdiparams", "inference.yml")


def download(url: str, dest: Path) -> None:
    dest.parent.mkdir(parents=True, exist_ok=True)
    if dest.exists() and dest.stat().st_size > 100:
        print(f"  cached {dest}")
        return
    print(f"  GET {url}")
    req = Request(url, headers={"User-Agent": "PdfToMarkdown-fetch/1.0"})
    with urlopen(req, timeout=180) as resp:
        data = resp.read()
    tmp = dest.with_suffix(dest.suffix + ".partial")
    tmp.write_bytes(data)
    tmp.replace(dest)


def main() -> int:
    dest_root = Path(sys.argv[1]) if len(sys.argv) > 1 else OUT
    for folder, repo in REPOS.items():
        for name in FILES:
            url = HF.format(repo=repo, file=name)
            download(url, dest_root / folder / name)
        marker = dest_root / folder / "inference.json"
        if not marker.exists():
            raise SystemExit(f"missing {marker}")
    print(f"Models ready under {dest_root}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
