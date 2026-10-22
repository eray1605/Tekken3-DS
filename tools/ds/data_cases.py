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
    mesh_<costume>.bin     the converted model (ds/nitrofs/models/) posed by the first frames of
                           its skeleton cases: corner positions and light levels (fighter_mesh)

motion_<bank>.bin (little-endian): char[4] "T3MB", u32 version, u32 stream count, per stream:
u32 offset (section 6), u32 frame count; then s16 pose[49] per frame, the streams in that order.

mesh_<costume>.bin: char[4] "T3MS", u32 version, u32 frame count, s32 light[3], s32 ambient,
s32 diffuse, u32 corner count, u32 normal count; per frame: the root and 22 joints (s16 rotation[9],
s32 translation[3]), s32 position[3] per corner (relative to the root), u8 level per normal.

    python3 tools/ds/data_cases.py OUT_DIR [--imported DIR] [--nitrofs DIR] [--seed N]
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
DEFAULT_NITROFS = ROOT / "ds" / "nitrofs"
CHANNELS = core_cases.CHANNELS
# Costumes of the smallest (45 %), a plain and the largest scale (106 %), each with poses of
# another bank.
SKELETONS = [("costume_42", "divmot19"), ("costume_00", "divmot00"), ("costume_06", "divmot99")]
SKELETON_FRAMES = 3000
MESH_COSTUME = "costume_00"
MESH_FRAMES = 60
MESH_LIGHT = (-1365, -2731, -2731)   # 4.12, towards the light
MESH_AMBIENT, MESH_DIFFUSE = 13, 20
JOINTS = 22


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


def read_model(data: bytes) -> tuple[list, list, list]:
    """Vertices and normals (joint, x, y, z) and corners (a, b, normal, weight) of a model file
    (tools/ds_import/models.py)."""
    vertices, normals, materials, batches, corners = struct.unpack_from("<HHHHI", data, 8)
    pos = 20
    points = [struct.unpack_from("<Bx3h", data, pos + 8 * i) for i in range(vertices + normals)]
    pos += 8 * (vertices + normals)
    for _ in range(materials):
        _, _, _, _, palette, texels = struct.unpack_from("<BBHHHI", data, pos)
        pos += 12 + 2 * palette + texels
    pos += 12 * batches
    corner_list = [struct.unpack_from("<HHHB", data, pos + 10 * c) for c in range(corners)]
    return points[:vertices], points[vertices:], corner_list


def frames_of(skeleton: bytes, count: int) -> list[list[tuple[list[int], list[int]]]]:
    """The first `count` frames' root and joints (rotation, translation) of a skeleton case file."""
    pos = 12 + 16 * JOINTS
    frames = struct.unpack_from("<I", skeleton, pos)[0]
    pos += 4
    out = []
    for _ in range(min(count, frames)):
        pos += 2 * CHANNELS + 20
        joints = []
        for _ in range(1 + JOINTS):
            rot = list(struct.unpack_from("<9h", skeleton, pos))
            t = list(struct.unpack_from("<3i", skeleton, pos + 18))
            joints.append((rot, t))
            pos += 30
        out.append(joints)
    return out


def rotate(rot: list[int], p: tuple[int, int, int]) -> list[int]:
    return [(rot[3 * k] * p[0] + rot[3 * k + 1] * p[1] + rot[3 * k + 2] * p[2]) >> 12 for k in range(3)]


def mesh_case_file(model: bytes, skeleton: bytes) -> bytes:
    vertices, normals, corners = read_model(model)
    frames = frames_of(skeleton, MESH_FRAMES)
    out = bytearray(b"T3MS" + struct.pack("<II3iiiII", 1, len(frames), *MESH_LIGHT, MESH_AMBIENT, MESH_DIFFUSE,
                                          len(corners), len(normals)))
    for (root_rot, root_t), *joints in frames:
        out += struct.pack("<9h3i", *root_rot, *root_t)
        for rot, t in joints:
            out += struct.pack("<9h3i", *rot, *t)
        posed = []
        for joint, *p in vertices:
            rot, t = joints[joint]
            d = rotate(rot, p)
            posed.append([t[k] + d[k] - root_t[k] for k in range(3)])
        for a, b, _, weight in corners:
            out += struct.pack("<3i", *(posed[a][k] + (((posed[b][k] - posed[a][k]) * weight) >> 8) for k in range(3)))
        for joint, *n in normals:
            w = rotate(joints[joint][0], n)
            cosine = sum(w[k] * MESH_LIGHT[k] for k in range(3)) >> 12
            level = MESH_AMBIENT + ((cosine * MESH_DIFFUSE) >> 12 if cosine > 0 else 0)
            out.append(min(level, 31))
    return bytes(out)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("out_dir", type=Path)
    parser.add_argument("--imported", type=Path, default=DEFAULT_IMPORTED)
    parser.add_argument("--nitrofs", type=Path, default=DEFAULT_NITROFS)
    parser.add_argument("--seed", type=int, default=5)
    args = parser.parse_args()
    manifest = json.loads((args.imported / "manifest.json").read_text())
    rng = random.Random(args.seed)
    args.out_dir.mkdir(parents=True, exist_ok=True)
    for bank in manifest["motion_banks"]:
        (args.out_dir / f"motion_{bank}.bin").write_bytes(motion_case_file(args.imported, bank))
    for costume, bank in SKELETONS:
        (args.out_dir / f"skeleton_{costume}.bin").write_bytes(skeleton_case_file(args.imported, costume, bank, rng))
    model = args.nitrofs / "models" / f"{MESH_COSTUME}.bin"
    if model.is_file():
        skeleton = (args.out_dir / f"skeleton_{MESH_COSTUME}.bin").read_bytes()
        (args.out_dir / f"mesh_{MESH_COSTUME}.bin").write_bytes(mesh_case_file(model.read_bytes(), skeleton))
    return 0


if __name__ == "__main__":
    sys.exit(main())
