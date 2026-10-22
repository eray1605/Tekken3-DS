#!/usr/bin/env python3
"""Synthetic pose cases for the DS core's host tests, without game data.

Builds pose tables of the game's shape from mathematics (a 4.12 sine, SquareRoot0 mantissas, a
limb basis and skeleton offsets of plausible size), places them at their Rev.1 addresses in a
blank executable image, and runs random pose vectors through the verified Python port
(tools/research/pose.py, bit-exact with the game's PoseBuildMatrices). The DS core must give the
same slots for the same tables.

Writes into OUT_DIR:
    pose_cases_tables.bin  the tables in the DS format (tools/ds_import/pose_tables.py)
    pose_cases.bin         the cases in the pose trace format (tools/research/export_traces.py)

    python3 tools/ds/pose_cases.py OUT_DIR [--seed N] [--anims N] [--frames N]
"""

from __future__ import annotations

import argparse
import math
import random
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "research"))
sys.path.insert(0, str(ROOT / "tools" / "ds_import"))

import pose  # noqa: E402
import pose_tables  # noqa: E402

TRACE_MAGIC = b"T3PT"
TRACE_VERSION = 1
CHANNELS = 49
# The slots the Euler joints write: all but the IK limbs' upper and lower slots.
EULER_SLOTS = [0, 1, 2, 3, 6, 7, 10, 11, 14, 17]


def synthetic_tables(rng: random.Random) -> dict:
    sin = [round(4096 * math.sin(2 * math.pi * i / 4096)) for i in range(pose_tables.SIN_ENTRIES)]
    # SquareRoot0: sqrt(a) = mant << ((31 − lz) >> 1) >> 12 with x = a's top bits in 0x40…0xFF.
    sqrt = [round(512 * math.sqrt(x)) for x in range(0x40, 0x100)]
    basis = euler(rng, sin)
    return {
        "sin": sin,
        "sqrt": sqrt,
        "euler_slots": EULER_SLOTS,
        "limb_basis": basis,
        "skeleton_offsets": [0, -220, 0, -140, -40, 0, 140, -40, 0, 0, 30, 0],
        "limb_offsets": [rng.randint(-60, 60) for _ in range(12)],
    }


def euler(rng: random.Random, sin: list[int]) -> list[int]:
    """A rotation from the port's own EulerToMatrix for random angles."""
    exe = blank_exe({"sin": sin})
    m = pose.euler_to_matrix(*(rng.randint(-0x8000, 0x7FFF) for _ in range(3)), pose.ExeTables(bytes(exe)))
    return pose.flat(m)


def blank_exe(tables: dict) -> bytearray:
    end = pose.SQRT_TABLE + 2 * pose_tables.SQRT_ENTRIES
    exe = bytearray(end - pose.EXE_BASE + pose.EXE_HEADER)

    def put(addr: int, fmt: str, values: list[int]) -> None:
        struct.pack_into(f"<{len(values)}{fmt}", exe, addr - pose.EXE_BASE + pose.EXE_HEADER, *values)

    put(pose.SIN_TABLE, "h", tables["sin"])
    if "sqrt" in tables:
        put(pose.SQRT_TABLE, "h", tables["sqrt"])
        put(pose.EULER_SLOTS, "B", tables["euler_slots"])
        put(pose.LIMB_BASIS, "h", tables["limb_basis"])
        put(pose.SKELETON_OFFSETS, "i", tables["skeleton_offsets"])
        put(pose.LIMB_OFFSETS, "i", tables["limb_offsets"])
    return exe


def random_pose(rng: random.Random) -> list[int]:
    """Root translation, four IK targets around the root (arms from inside the minimum reach to
    beyond the maximum, legs likewise), swivels and Euler angles over the whole turn."""
    root = [rng.randint(-1500, 1500) for _ in range(3)]
    vec = root[:]
    for _ in range(4):
        vec += [root[k] + rng.randint(-1100, 1100) for k in range(3)]
    vec += [rng.randint(-0x8000, 0x7FFF) for _ in range(4 + 30)]
    return vec


def cases(tables: dict, rng: random.Random, anims: int, frames: int) -> bytes:
    tab = pose.ExeTables(bytes(blank_exe(tables)))
    out = bytearray(TRACE_MAGIC + struct.pack("<II", TRACE_VERSION, anims))
    for a in range(anims):
        out += struct.pack("<II", a, frames)
        slots = [[pose.ONE, 0, 0, 0, pose.ONE, 0, 0, 0, pose.ONE] for _ in range(pose.SLOT_COUNT)]
        for _ in range(frames):
            vec = random_pose(rng)
            pose.pose_build_matrices(vec, slots, tab)
            out += struct.pack(f"<{CHANNELS}h", *vec)
            for slot in slots:
                out += struct.pack("<9h", *slot)
    return bytes(out)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("out_dir", type=Path)
    parser.add_argument("--seed", type=int, default=3)
    parser.add_argument("--anims", type=int, default=40)
    parser.add_argument("--frames", type=int, default=60)
    args = parser.parse_args()
    rng = random.Random(args.seed)
    tables = synthetic_tables(rng)
    args.out_dir.mkdir(parents=True, exist_ok=True)
    (args.out_dir / "pose_cases_tables.bin").write_bytes(pose_tables.encode(tables))
    (args.out_dir / "pose_cases.bin").write_bytes(cases(tables, rng, args.anims, args.frames))
    return 0


if __name__ == "__main__":
    sys.exit(main())
