#!/usr/bin/env python3
"""Cases from the converted game data for the DS core's host tests (tools/ds/run_tests.sh).

Needs the remake's converted tree (`make convert`). Writes into OUT_DIR (game data: it lies under
ds/build/, which is git-ignored):

    motion_<bank>.bin      the remake's baked poses of a bank (motion/<bank>.poses.bin.gz, every
                           frame of every stream it bakes), against which the DS samples the bank's
                           own streams (ds/nitrofs/motion/<bank>.bin)
    skeleton_<costume>.bin a costume's rig (its model.json) with the converted pose tables, baked
                           poses of a bank and random placements through the Python ports
                           (core_cases.skeleton_case_file)

motion_<bank>.bin (little-endian): char[4] "T3MB", u32 version, u32 stream count, per stream:
u32 offset (section 6), u32 frame count; then s16 pose[49] per frame, the streams in that order.

    python3 tools/ds/data_cases.py OUT_DIR [--imported DIR] [--seed N]
"""

from __future__ import annotations

import argparse
import gzip
import json
import random
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "ds"))

import core_cases  # noqa: E402  (puts tools/research and tools/ds_import on the import path)
import rigs  # noqa: E402

DEFAULT_IMPORTED = ROOT / "remake" / "imported"
CHANNELS = core_cases.CHANNELS
# Costumes of the smallest (45 %), a plain and the largest scale (106 %), each with poses of
# another bank.
SKELETONS = [("costume_42", "divmot19"), ("costume_00", "divmot00"), ("costume_06", "divmot99")]
SKELETON_FRAMES = 3000


def baked(imported: Path, bank: str) -> tuple[list[tuple[int, int, int]], bytes]:
    """The streams (offset, first pose, frames) a bank's baked poses hold, in pose order, and the poses."""
    index = json.loads(gzip.decompress((imported / "motion" / f"{bank}.json.gz").read_bytes()))
    anims = sorted((first, int(offset), frames) for offset, (first, frames, *_) in index["anims"].items())
    poses = gzip.decompress((imported / "motion" / f"{bank}.poses.bin.gz").read_bytes())
    if len(poses) != 2 * CHANNELS * index["frames"]:
        raise ValueError(f"{bank}: {len(poses)} bytes of poses for {index['frames']} frames")
    return [(offset, first, frames) for first, offset, frames in anims], poses


def motion_case_file(imported: Path, bank: str) -> bytes:
    anims, poses = baked(imported, bank)
    out = bytearray(b"T3MB" + struct.pack("<II", 1, len(anims)))
    for offset, _, frames in anims:
        out += struct.pack("<II", offset, frames)
    return bytes(out) + poses


def skeleton_case_file(imported: Path, costume: str, bank: str, rng: random.Random) -> bytes:
    tables = json.loads((imported / "tables" / "pose.json").read_text())
    rig = rigs.rig(json.loads((imported / "characters" / costume / "model.json").read_text()))
    _, poses = baked(imported, bank)
    frames = min(SKELETON_FRAMES, len(poses) // (2 * CHANNELS))

    def inputs():
        for f in range(frames):
            yield list(struct.unpack_from(f"<{CHANNELS}h", poses, 2 * CHANNELS * f)), *core_cases.random_placement(rng)

    return core_cases.skeleton_case_file(bytes(core_cases.blank_exe(tables)), rig, frames, inputs())


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("out_dir", type=Path)
    parser.add_argument("--imported", type=Path, default=DEFAULT_IMPORTED)
    parser.add_argument("--seed", type=int, default=5)
    args = parser.parse_args()
    manifest = json.loads((args.imported / "manifest.json").read_text())
    rng = random.Random(args.seed)
    args.out_dir.mkdir(parents=True, exist_ok=True)
    for bank in manifest["motion_banks"]:
        (args.out_dir / f"motion_{bank}.bin").write_bytes(motion_case_file(args.imported, bank))
    for costume, bank in SKELETONS:
        (args.out_dir / f"skeleton_{costume}.bin").write_bytes(skeleton_case_file(args.imported, costume, bank, rng))
    return 0


if __name__ == "__main__":
    sys.exit(main())
