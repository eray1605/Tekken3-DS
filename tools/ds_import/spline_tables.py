"""The B-spline weight and index tables of the animation streams for the DS: the executable's
0x80096B00…0x80097160 (motion.SplineTables, AnimSplineSample) into `tables/spline.bin`
(read by ds/source/content/spline_file.c).

    char[4] "T3SL", u32 version, u8 tables[0x660]; the code's base address 0x80096B40 lies at 0x40.
"""

from __future__ import annotations

import struct
from pathlib import Path

MAGIC = b"T3SL"
VERSION = 1
START = 0x80096B00
END = 0x80097160
TARGET = "tables/spline.bin"


def encode(tables: bytes) -> bytes:
    if len(tables) != END - START:
        raise ValueError(f"spline tables: {len(tables)} bytes, not {END - START}")
    return MAGIC + struct.pack("<I", VERSION) + tables


def convert(disc, out: Path) -> list[Path]:
    """`disc`: a common.Disc in Japan Rev.1's layout (its `exe_bytes(addr, size)`)."""
    target = out / TARGET
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_bytes(encode(disc.exe_bytes(START, END - START)))
    return [target]
