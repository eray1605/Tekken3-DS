#!/usr/bin/env python3
"""Synthetic cases for the DS core's host tests, without game data.

Builds tables of the game's shape from mathematics (a 4.12 sine, SquareRoot0 mantissas, a limb
basis, skeleton offsets of plausible size, B-spline tables), places them at their Rev.1 addresses
in a blank executable image, and runs random inputs through the verified Python ports
(tools/research/pose.py, bit-exact with PoseBuildMatrices; motion.py, bit-exact with
AnimDecodePose; fight_sim.root_rotation, FUN_8003A6B4). The DS core must give the same results for
the same tables.

Writes into OUT_DIR:
    pose_cases_tables.bin  the pose tables in the DS format (tools/ds_import/pose_tables.py)
    pose_cases.bin         pose vectors and slots, in the pose trace format (export_traces.py)
    motion_cases.bin       spline tables, animation streams and their sampled poses
    skeleton_cases.bin     a rig, skeleton inputs per frame and the joints they give

    python3 tools/ds/core_cases.py OUT_DIR [--seed N]

motion_cases.bin (little-endian): char[4] "T3MC", u32 version, u32 table size, the spline tables
(the executable's 0x80096B00…0x80097160), u32 stream count, per stream: u32 byte count, the stream
(header and keys), s16 pose[49] per frame.

skeleton_cases.bin: char[4] "T3SK", u32 version, s32 scale, per joint (22): s8 parent, 3 pad bytes,
s32 offset[3]; u32 frame count, per frame: s16 pose[49], s32 anchor[3], s32 facing, s32 heading,
then the result: s16 root rotation[9], s32 root translation[3], per joint s16 rotation[9],
s32 translation[3]. The frames run on one skeleton: its local slots carry over.
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

import fight_math  # noqa: E402
import fight_sim  # noqa: E402
import motion  # noqa: E402
import pose  # noqa: E402
import pose_tables  # noqa: E402

TRACE_MAGIC = b"T3PT"
TRACE_VERSION = 1
CHANNELS = 49
# The slots the Euler joints write: all but the IK limbs' upper and lower slots.
EULER_SLOTS = [0, 1, 2, 3, 6, 7, 10, 11, 14, 17]
SPLINE_START = motion.SPLINE_TABLE_BASE - 0x40
SPLINE_BYTES = 0x660
# Spline index entries (u8, scaled by two into the weight tables) lie below base + 0x200.
SPLINE_INDEX_END = motion.SPLINE_TABLE_BASE + 0x200
SKELETON_JOINTS = 18
ALL_JOINTS = 22


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


def synthetic_spline(rng: random.Random) -> bytes:
    """Random spline tables whose index entries stay inside the weight tables, as the game's do."""
    return bytes(rng.randint(0, 0x7F) if SPLINE_START + i < SPLINE_INDEX_END else rng.randint(0, 0xFF)
                 for i in range(SPLINE_BYTES))


def blank_exe(tables: dict, spline: bytes = b"") -> bytearray:
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
    if spline:
        put(SPLINE_START, "B", list(spline))
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


def identity() -> list[int]:
    return [pose.ONE, 0, 0, 0, pose.ONE, 0, 0, 0, pose.ONE]


def pose_cases(tables: dict, rng: random.Random, anims: int = 40, frames: int = 60) -> bytes:
    tab = pose.ExeTables(bytes(blank_exe(tables)))
    out = bytearray(TRACE_MAGIC + struct.pack("<II", TRACE_VERSION, anims))
    for a in range(anims):
        out += struct.pack("<II", a, frames)
        slots = [identity() for _ in range(pose.SLOT_COUNT)]
        for _ in range(frames):
            vec = random_pose(rng)
            pose.pose_build_matrices(vec, slots, tab)
            out += struct.pack(f"<{CHANNELS}h", *vec)
            for slot in slots:
                out += struct.pack("<9h", *slot)
    return bytes(out)


# ------------------------------------------------------------------------------------- motion

def spline_keys(rng: random.Random, frames: int) -> list[int]:
    """A keyed channel: a base value, then one word per segment (delta << 5 | length − 1) whose
    lengths add up to the last frame, with at least two segments."""
    while True:
        lengths = []
        left = frames - 1
        while left:
            step = rng.randint(1, min(32, left))
            lengths.append(step)
            left -= step
        if len(lengths) >= 2:
            break
    return [rng.randint(-0x8000, 0x7FFF)] + [(rng.randint(-1024, 1023) << 5) | (n - 1) for n in lengths]


def random_stream(rng: random.Random) -> bytes:
    frames = rng.choice([1, 2, 3, 9, 10, rng.randint(3, 64), rng.randint(64, 255)])
    channels = []
    for _ in range(CHANNELS):
        kinds = ["none", "constant", "linear", "full"] + (["spline"] * 4 if frames >= 3 else [])
        kind = rng.choice(kinds)
        if kind == "none":
            keys = []
        elif kind == "constant":
            keys = [rng.randint(-0x8000, 0x7FFF)]
        elif kind == "linear":
            keys = [rng.randint(-0x8000, 0x7FFF), rng.randint(-0x8000, 0x7FFF)]
        elif kind == "full":
            keys = [rng.randint(-0x8000, 0x7FFF) for _ in range(frames)]
        else:
            keys = spline_keys(rng, frames)
        channels.append(keys)
    counts = [len(k) for k in channels]
    if max(counts) > 255:
        raise ValueError("a channel has more than 255 keys")
    head = struct.pack("<H", frames | counts[0] << 8) + bytes(counts[1:])
    body = b"".join(struct.pack(f"<{len(k)}h", *k) for k in channels)
    return head + body


def motion_cases(rng: random.Random, streams: int = 300) -> bytes:
    spline = synthetic_spline(rng)
    tab = motion.SplineTables(bytes(blank_exe({"sin": [0] * pose_tables.SIN_ENTRIES}, spline)))
    out = bytearray(b"T3MC" + struct.pack("<II", 1, len(spline)) + spline + struct.pack("<I", streams))
    for _ in range(streams):
        data = random_stream(rng)
        stream = motion.parse_anim(data, 0)
        out += struct.pack("<I", len(data)) + data
        for frame in range(stream.frames):
            out += struct.pack(f"<{CHANNELS}h", *motion.sample_pose(stream, frame, tab))
    return bytes(out)


# ----------------------------------------------------------------------------------- skeleton

def random_rig(rng: random.Random) -> tuple[int, list[int], list[list[int]]]:
    """A joint tree (parents before children, attachments on skeleton joints), offsets and scale."""
    parents = [-1] + [rng.randrange(0, m) for m in range(1, SKELETON_JOINTS)]
    parents += [rng.randrange(0, SKELETON_JOINTS) for _ in range(SKELETON_JOINTS, ALL_JOINTS)]
    offsets = [[rng.randint(-300, 300) for _ in range(3)] for _ in range(ALL_JOINTS)]
    scale = rng.randint(80, 120) * 0x1000 // 100
    return scale, parents, offsets


def skeleton_cases(tables: dict, rng: random.Random, frames: int = 400) -> bytes:
    def inputs():
        for _ in range(frames):
            vec = random_pose(rng)
            vec[:3] = [rng.randint(-3000, 3000) for _ in range(3)]
            yield vec, *random_placement(rng)

    return skeleton_case_file(bytes(blank_exe(tables)), random_rig(rng), frames, inputs())


def random_placement(rng: random.Random) -> tuple[list[int], int, int]:
    """An anchor, a facing and a heading for FighterSkeleton.update."""
    anchor = [rng.randint(-20000, 20000), rng.randint(-2000, 2000), rng.randint(-20000, 20000)]
    return anchor, rng.randint(0, 0xFFFF), rng.randint(0, 0xFFFF)


def skeleton_case_file(exe: bytes, rig: tuple[int, list[int], list[list[int]]], frames: int, inputs) -> bytes:
    """skeleton_cases.bin for a rig (scale, parents, offsets) and `frames` inputs (pose vector,
    anchor, facing, heading), the pose tables in `exe` at their Rev.1 addresses."""
    tab = pose.ExeTables(exe)
    fight_sim._EXE = fight_math.Exe(exe)
    scale, parents, offsets = rig
    out = bytearray(b"T3SK" + struct.pack("<Ii", 1, scale))
    for m in range(ALL_JOINTS):
        out += struct.pack("<b3x3i", parents[m], *offsets[m])
    out += struct.pack("<I", frames)
    slots = [identity() for _ in range(SKELETON_JOINTS)]
    joints = [pose.Frame(pose.unflat(identity()), [0, 0, 0]) for _ in range(ALL_JOINTS)]
    for vec, anchor, facing, heading in inputs:
        out += struct.pack(f"<{CHANNELS}h3iii", *vec, *anchor, facing, heading)
        # FighterSkeleton.update: the pose, the root (RootUpdate on the ground), then the joints.
        pose.pose_build_matrices(vec, slots, tab)
        d = [pose.s16((vec[i] * scale) >> 12) for i in range(3)]
        a = ((0x8000 - facing) >> 4) & 0xFFF
        c = fight_math.trig_raw(a, fight_sim.exe(), fight_math.COS_TABLE)
        s = fight_math.trig_raw(a, fight_sim.exe())
        root = pose.Frame(pose.unflat(fight_sim.root_rotation(0, (heading - 0x8000) & 0xFFFF, 0)), [
            anchor[0] + fight_math.trunc12(d[0] * c - d[2] * s),
            anchor[1] + d[1],
            anchor[2] + fight_math.trunc12(d[0] * s + d[2] * c),
        ])
        for m in range(ALL_JOINTS):
            parent = root if parents[m] < 0 else joints[parents[m]]
            local = slots[m] if m < SKELETON_JOINTS else identity()
            joints[m] = pose.compose_joint_offset(parent, pose.unflat(local), offsets[m])
        for frame in [root, *joints]:
            out += struct.pack("<9h3i", *pose.flat(frame.rot), *frame.t)
    return bytes(out)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("out_dir", type=Path)
    parser.add_argument("--seed", type=int, default=3)
    args = parser.parse_args()
    rng = random.Random(args.seed)
    tables = synthetic_tables(rng)
    args.out_dir.mkdir(parents=True, exist_ok=True)
    (args.out_dir / "pose_cases_tables.bin").write_bytes(pose_tables.encode(tables))
    (args.out_dir / "pose_cases.bin").write_bytes(pose_cases(tables, rng))
    (args.out_dir / "motion_cases.bin").write_bytes(motion_cases(rng))
    (args.out_dir / "skeleton_cases.bin").write_bytes(skeleton_cases(tables, rng))
    return 0


if __name__ == "__main__":
    sys.exit(main())
