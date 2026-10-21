#!/usr/bin/env python3
"""Generate xlsx/csv fixtures for tst_import (stdlib only)."""

import csv
import zipfile
from pathlib import Path

OUT = Path(__file__).resolve().parent

CONTENT_TYPES = b"""<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">
<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>
<Default Extension="xml" ContentType="application/xml"/>
<Override PartName="/xl/workbook.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml"/>
<Override PartName="/xl/worksheets/sheet1.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml"/>
<Override PartName="/xl/sharedStrings.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.sharedStrings+xml"/>
</Types>
"""

RELS_ROOT = b"""<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="xl/workbook.xml"/>
</Relationships>
"""

WORKBOOK = b"""<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<workbook xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main" xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships">
<sheets><sheet name="Sheet1" sheetId="1" r:id="rId1"/></sheets>
</workbook>
"""

WB_RELS = b"""<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="worksheets/sheet1.xml"/>
<Relationship Id="rId2" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/sharedStrings" Target="sharedStrings.xml"/>
</Relationships>
"""


def escape_xml(s: str) -> str:
    return (
        s.replace("&", "&amp;")
        .replace("<", "&lt;")
        .replace(">", "&gt;")
        .replace('"', "&quot;")
    )


def build_shared_strings(strings: list[str]) -> bytes:
    items = []
    for s in strings:
        items.append(f"<si><t>{escape_xml(s)}</t></si>")
    body = "".join(items)
    return (
        f'<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'
        f'<sst xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main" '
        f'count="{len(strings)}" uniqueCount="{len(strings)}">{body}</sst>'
    ).encode("utf-8")


def col_name(index: int) -> str:
    name = ""
    n = index
    while True:
        name = chr(ord("A") + n % 26) + name
        n = n // 26 - 1
        if n < 0:
            break
    return name


def cell_ref(row_1based: int, col_0based: int) -> str:
    return f"{col_name(col_0based)}{row_1based}"


def build_sheet(rows: list[list]) -> bytes:
    """rows: list of list of ('s', str_index) | ('n', number_str) | ('inline', str)"""
    row_xml = []
    for r_idx, row in enumerate(rows, start=1):
        cells = []
        for c_idx, cell in enumerate(row):
            ref = cell_ref(r_idx, c_idx)
            kind = cell[0]
            if kind == "s":
                cells.append(f'<c r="{ref}" t="s"><v>{cell[1]}</v></c>')
            elif kind == "n":
                cells.append(f'<c r="{ref}"><v>{cell[1]}</v></c>')
            elif kind == "inline":
                t = escape_xml(cell[1])
                cells.append(
                    f'<c r="{ref}" t="inlineStr"><is><t>{t}</t></is></c>'
                )
            else:
                raise ValueError(kind)
        row_xml.append(f'<row r="{r_idx}">{"".join(cells)}</row>')
    body = "".join(row_xml)
    return (
        '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'
        '<worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main">'
        f'<sheetData>{body}</sheetData></worksheet>'
    ).encode("utf-8")


def write_xlsx(path: Path, shared: list[str], sheet_rows: list[list]) -> None:
    with zipfile.ZipFile(path, "w", compression=zipfile.ZIP_DEFLATED) as zf:
        zf.writestr("[Content_Types].xml", CONTENT_TYPES)
        zf.writestr("_rels/.rels", RELS_ROOT)
        zf.writestr("xl/workbook.xml", WORKBOOK)
        zf.writestr("xl/_rels/workbook.xml.rels", WB_RELS)
        zf.writestr("xl/sharedStrings.xml", build_shared_strings(shared))
        zf.writestr("xl/worksheets/sheet1.xml", build_sheet(sheet_rows))


def ss_index(shared: list[str], text: str) -> int:
    if text not in shared:
        shared.append(text)
    return shared.index(text)


def make_devices_ok() -> None:
    shared: list[str] = []
    header = [
        ("s", ss_index(shared, "tailscale_ip")),
        ("s", ss_index(shared, "phone")),
        ("s", ss_index(shared, "port")),
    ]
    rows = [
        header,
        [
            ("s", ss_index(shared, "100.64.1.1")),
            ("s", ss_index(shared, "9111111111")),
            ("s", ss_index(shared, "5555")),
        ],
        [
            ("s", ss_index(shared, "100.64.1.2")),
            ("s", ss_index(shared, "9222222222")),
            ("s", ss_index(shared, "5555")),
        ],
        [
            ("s", ss_index(shared, "100.64.1.3")),
            ("s", ss_index(shared, "9333333333")),
            ("s", ss_index(shared, "5555")),
        ],
    ]
    write_xlsx(OUT / "devices_ok.xlsx", shared, rows)

    csv_rows = [
        ["tailscale_ip", "phone", "port"],
        ["100.64.1.1", "9111111111", "5555"],
        ["100.64.1.2", "9222222222", "5555"],
        ["100.64.1.3", "9333333333", "5555"],
    ]
    with (OUT / "devices_ok.csv").open("w", encoding="utf-8", newline="") as f:
        csv.writer(f).writerows(csv_rows)


def make_numeric_phone() -> None:
    shared: list[str] = []
    header = [
        ("s", ss_index(shared, "ip")),
        ("s", ss_index(shared, "phone")),
    ]
    rows = [
        header,
        [
            ("s", ss_index(shared, "100.64.2.1")),
            ("n", "9.8765E+9"),
        ],
        [
            ("s", ss_index(shared, "100.64.2.2")),
            ("inline", "+91 98765-43210"),
        ],
    ]
    write_xlsx(OUT / "devices_numeric_phone.xlsx", shared, rows)


def main() -> None:
    make_devices_ok()
    make_numeric_phone()
    print("Wrote fixtures to", OUT)


if __name__ == "__main__":
    main()
