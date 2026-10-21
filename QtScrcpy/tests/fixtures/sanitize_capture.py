#!/usr/bin/env python3
"""Sanitize uiautomator XML / adb content query captures for test fixtures."""

import re
import sys
from pathlib import Path

PHONE_RE = re.compile(r"\+?\d{7,15}")
SIX_DIGIT_IN_SMS = re.compile(r"\(\d{6}\)")


def sanitize_xml(text: str) -> str:
    text = PHONE_RE.sub("9876543210", text)
    return text


def sanitize_sms_rows(text: str) -> str:
    lines = []
    for line in text.splitlines():
        if not line.strip().startswith("Row:"):
            lines.append(line)
            continue
        line = SIX_DIGIT_IN_SMS.sub("(482913)", line)
        line = PHONE_RE.sub("9876543210", line)
        lines.append(line)
    return "\n".join(lines) + ("\n" if text.endswith("\n") else "")


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: sanitize_capture.py <in> <out>", file=sys.stderr)
        return 2
    src = Path(sys.argv[1]).read_text(encoding="utf-8", errors="replace")
    out_path = Path(sys.argv[2])
    if out_path.suffix == ".xml":
        cleaned = sanitize_xml(src)
    else:
        cleaned = sanitize_sms_rows(src)
    out_path.write_text(cleaned, encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
