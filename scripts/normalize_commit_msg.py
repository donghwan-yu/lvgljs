#!/usr/bin/env python3
"""Normalize .git-commit-msg-tmp.txt for git commit -F (ASCII-only, UTF-8 no BOM).

AI editors and shells can write smart punctuation or invalid UTF-8 into the temp
message file. Git then warns "commit message did not conform to UTF-8" and may
store mojibake (e.g. em dash becomes Latin-1 bytes \\xa1\\xaa).

Rules (full file; see lib/normalize_common.py):
  - U+2014 em dash and U+2013 en dash -> ASCII hyphen (-)
  - Smart quotes, ellipsis, NBSP, and common mojibake -> ASCII equivalents
  - Strip control characters except tab and newline
  - Fail if any non-ASCII remains after normalization

Usage:
  python scripts/normalize_commit_msg.py
  python scripts/normalize_commit_msg.py --path .git-commit-msg-tmp.txt
  python scripts/normalize_commit_msg.py --check
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

_SCRIPTS = Path(__file__).resolve().parent
if str(_SCRIPTS) not in sys.path:
    sys.path.insert(0, str(_SCRIPTS))

from lib.normalize_common import (
    ROOT,
    normalize_newlines,
    decode_text_bytes,
    normalize_commit_message,
    strip_control_chars,
)

DEFAULT_PATH = ROOT / ".git-commit-msg-tmp.txt"


def on_disk_text(raw: bytes) -> str:
    text = decode_text_bytes(raw)
    return normalize_newlines(strip_control_chars(text))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--path",
        type=Path,
        default=DEFAULT_PATH,
        help=f"Commit message file (default: {DEFAULT_PATH.relative_to(ROOT).as_posix()})",
    )
    parser.add_argument(
        "--check",
        action="store_true",
        help="Exit 1 if the file would change or is not normalized",
    )
    args = parser.parse_args()

    path = args.path.resolve()
    if not path.is_file():
        print(f"{path}: file not found", file=sys.stderr)
        return 1

    raw = path.read_bytes()
    try:
        normalized, source_encoding = normalize_commit_message(raw)
    except ValueError as exc:
        print(f"{path}: {exc}", file=sys.stderr)
        return 1

    current = on_disk_text(raw)
    would_change = normalized != current

    if args.check:
        if would_change:
            print(
                "commit message needs normalization "
                "(run: python scripts/normalize_commit_msg.py)",
                file=sys.stderr,
            )
            return 1
        print(f"commit message OK ({path.relative_to(ROOT).as_posix()})")
        return 0

    if would_change:
        path.write_text(normalized, encoding="utf-8", newline="\n")
        print(
            f"Normalized {path.relative_to(ROOT).as_posix()} "
            f"(from {source_encoding} to UTF-8, no BOM, ASCII-only)"
        )
    else:
        print(
            f"Commit message already normalized ({path.relative_to(ROOT).as_posix()})"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
