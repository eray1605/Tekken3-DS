"""The pose tables for the DS: `tables/pose.json` of the remake's converted tree into
`tables/pose.bin` (read by ds/source/content/pose_tables_file.c).

    char[4] "T3PS", u32 version, s16 sin[5120], s16 sqrt[192], u8 euler_slots[10], 2 pad bytes,
    s16 limb_basis[9], 2 pad bytes, s32 skeleton_offsets[12], s32 limb_offsets[12]; little-endian.
"""

from __future__ import annotations

import json
import struct
from pathlib import Path

MAGIC = b"T3PS"
VERSION = 1
SIN_ENTRIES = 4096 + 1024
SQRT_ENTRIES = 0x100 - 0x40
EULER_JOINTS = 10
SOURCE = "tables/pose.json"
TARGET = "tables/pose.bin"


def encode(tables: dict) -> bytes:
    """The file for the tables of pose.json (keys sin, sqrt, euler_slots, limb_basis,
    skeleton_offsets, limb_offsets)."""
    counts = {"sin": SIN_ENTRIES, "sqrt": SQRT_ENTRIES, "euler_slots": EULER_JOINTS, "limb_basis": 9,
              "skeleton_offsets": 12, "limb_offsets": 12}
    for key, count in counts.items():
        if len(tables[key]) != count:
            raise ValueError(f"pose tables: {key} has {len(tables[key])} entries, not {count}")
    return b"".join([
        MAGIC, struct.pack("<I", VERSION),
        struct.pack(f"<{SIN_ENTRIES}h", *tables["sin"]),
        struct.pack(f"<{SQRT_ENTRIES}h", *tables["sqrt"]),
        struct.pack(f"<{EULER_JOINTS}B2x", *tables["euler_slots"]),
        struct.pack("<9h2x", *tables["limb_basis"]),
        struct.pack("<12i", *tables["skeleton_offsets"]),
        struct.pack("<12i", *tables["limb_offsets"]),
    ])


def convert(imported: Path, out: Path) -> list[Path]:
    tables = json.loads((imported / SOURCE).read_text())
    target = out / TARGET
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_bytes(encode(tables))
    return [target]
