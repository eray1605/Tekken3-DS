"""Each costume's joint layout for the DS: `characters/<costume>/model.json` of the remake's converted
tree into `rigs/<costume>.bin` (read by ds/source/content/rig_file.c into a FighterRig), as
CharacterModel.joint_offsets, joint_parents and scale_fixed give it.

    char[4] "T3RG", u32 version, s32 scale (4.12), per joint (22): s8 parent (−1: the root),
    3 pad bytes, s32 offset x, y, z (the KMD's x, y, −z); little-endian.

Every parent comes before its child, as FighterSkeleton composes the joints in order.
"""

from __future__ import annotations

import json
import struct
from pathlib import Path

MAGIC = b"T3RG"
VERSION = 1
JOINTS = 22
ONE = 0x1000
SOURCE_DIR = "characters"
TARGET_DIR = "rigs"


def rig(model: dict) -> tuple[int, list[int], list[list[int]]]:
    """Scale, parent joint and offset per joint of a model.json."""
    parts = model["parts"]
    parents: list[int | None] = [None] * JOINTS
    offsets = [[0, 0, 0] for _ in range(JOINTS)]
    for part in parts:
        joint = part["joint"]
        if not 0 <= joint < JOINTS or parents[joint] is not None or not -1 <= part["parent"] < len(parts):
            raise ValueError(f"{model['name']}: joint {joint} out of range, given twice or with a bad parent")
        parents[joint] = parts[part["parent"]]["joint"] if part["parent"] >= 0 else -1
        x, y, z = part["offset"]
        offsets[joint] = [x, y, -z]
    for joint, parent in enumerate(parents):
        if parent is None or parent >= joint:
            raise ValueError(f"{model['name']}: joint {joint} has no part or a parent after it")
    return model["scale_percent"] * ONE // 100, parents, offsets


def encode(scale: int, parents: list[int], offsets: list[list[int]]) -> bytes:
    out = bytearray(MAGIC + struct.pack("<Ii", VERSION, scale))
    for parent, offset in zip(parents, offsets):
        out += struct.pack("<b3x3i", parent, *offset)
    return bytes(out)


def convert(imported: Path, out: Path) -> list[Path]:
    paths = []
    for source in sorted((imported / SOURCE_DIR).glob("costume_*/model.json")):
        target = out / TARGET_DIR / f"{source.parent.name}.bin"
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(encode(*rig(json.loads(source.read_text()))))
        paths.append(target)
    return paths
