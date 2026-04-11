from __future__ import annotations

import os
from typing import Iterable

from .protocol import MagicCliError


def hexdump_lines(data: bytes, base_offset: int = 0, width: int = 16) -> Iterable[str]:
    for off in range(0, len(data), width):
        chunk = data[off:off + width]
        hex_bytes = " ".join(f"{b:02X}" for b in chunk)
        hex_bytes = f"{hex_bytes:<{width * 3 - 1}}"
        ascii_bytes = "".join(chr(b) if 32 <= b <= 126 else "." for b in chunk)
        yield f"{base_offset + off:08X}  {hex_bytes}  |{ascii_bytes}|"


def hexdump_header_line(width: int = 16, show_ascii: bool = True) -> str:
    cols = " ".join(f"{i:02X}" for i in range(width))
    if show_ascii:
        return f"Offset (h)  {cols}  Decoded text"
    return f"Offset (h)  {cols}"


def print_hexdump(data: bytes, base_offset: int = 0, width: int = 16, show_header: bool = False) -> None:
    if show_header:
        print(hexdump_header_line(width=width, show_ascii=True))
        print("")
    for line in hexdump_lines(data, base_offset=base_offset, width=width):
        print(line)


def print_hexdump_legacy(
    data: bytes,
    base_addr: int = 0,
    width: int = 16,
    show_ascii: bool = True,
    show_header: bool = True,
) -> None:
    """Mimic the original firmware utils_format_hexdump_ex() layout."""
    if show_header:
        print(hexdump_header_line(width=width, show_ascii=show_ascii))
        print("")

    for off in range(0, len(data), width):
        chunk = data[off:off + width]
        line = f"0x{base_addr + off:08X}: "
        for i in range(width):
            if i < len(chunk):
                line += f"{chunk[i]:02X} "
            else:
                line += "   "
        if show_ascii:
            ascii_bytes = "".join(chr(b) if 32 <= b <= 126 else "." for b in chunk)
            line += f" {ascii_bytes}"
        print(line)


def hexdump_file(path: str, offset_raw: str = "0", length_raw: str = "256") -> None:
    if not os.path.exists(path):
        raise MagicCliError(f"File not found: {path}")

    offset = int(offset_raw, 0)
    length = int(length_raw, 0)
    if offset < 0 or length < 0:
        raise MagicCliError("offset/length must be >= 0")

    with open(path, "rb") as f:
        f.seek(offset)
        data = f.read(length)

    print(f"Hexdump: {path} @ 0x{offset:X}, {len(data)} bytes")
    print_hexdump(data, base_offset=offset)
