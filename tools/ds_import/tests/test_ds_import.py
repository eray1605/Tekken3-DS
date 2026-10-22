"""The DS converter's modules on a stub disc and synthetic models."""

from __future__ import annotations

import json
import random
import struct
import tempfile
import unittest
from pathlib import Path
from unittest import mock

import numpy as np

import characters
import convert
import models
import motion
import motion_banks
import rigs
import spline_tables


def _bank(record_id: int) -> bytes:
    """A bank of the minimal shape motion.parse_bank accepts, marked with its BNS id."""
    body = struct.pack("<I", record_id) * 4
    size = 0x40 + len(body)
    return bytes([0, 1]) + struct.pack("<H", 0) + struct.pack("<15I", 0x40, *([size] * 14)) + body


class StubDisc:
    """The parts of common.Disc the DS converter reads: BNS records and executable bytes."""

    def __init__(self, exe: dict[int, bytes] | None = None) -> None:
        self.exe = exe or {}
        self.read: list[tuple[str, int, int]] = []

    def bns(self, record_id: int) -> bytes:
        self.read.append(("bns", record_id, 0))
        return _bank(record_id)

    def exe_bytes(self, addr: int, size: int) -> bytes:
        self.read.append(("exe", addr, size))
        return self.exe[addr][:size]


def _model(name: str, scale: int, order: list[int], parents: dict[int, int], offsets: dict[int, list[int]]) -> dict:
    """A model.json with one part per joint, the parts in `order` (joints), parents by joint."""
    index = {joint: i for i, joint in enumerate(order)}
    parts = [{"joint": j, "row": i, "drawn": True, "offset": offsets[j],
              "parent": index[parents[j]] if parents[j] >= 0 else -1} for i, j in enumerate(order)]
    return {"name": name, "scale_percent": scale, "parts": parts}


def _random_model(rng: random.Random, name: str = "costume_00") -> tuple[dict, dict[int, int], dict[int, list[int]]]:
    parents = {0: -1, **{j: rng.randrange(-1, j) for j in range(1, rigs.JOINTS)}}
    offsets = {j: [rng.randint(-300, 300) for _ in range(3)] for j in range(rigs.JOINTS)}
    order = list(range(rigs.JOINTS))
    rng.shuffle(order)
    return _model(name, rng.randint(45, 106), order, parents, offsets), parents, offsets


class SplineTablesTest(unittest.TestCase):
    def test_reads_the_executable_range(self) -> None:
        blob = bytes(range(256)) * 6 + bytes(0x660 - 0x600)
        disc = StubDisc({spline_tables.START: blob})
        with tempfile.TemporaryDirectory() as tmp:
            [path] = spline_tables.convert(disc, Path(tmp))
            data = path.read_bytes()
        self.assertEqual(disc.read, [("exe", 0x80096B00, 0x660)])
        self.assertEqual(data[:8], b"T3SL" + struct.pack("<I", 1))
        self.assertEqual(data[8:], blob)
        # The code's base address lies at 0x40, as motion.SplineTables expects.
        self.assertEqual(spline_tables.START + 0x40, motion.SPLINE_TABLE_BASE)

    def test_rejects_a_short_table(self) -> None:
        with self.assertRaisesRegex(ValueError, "not 1632"):
            spline_tables.encode(bytes(0x600))


class MotionBanksTest(unittest.TestCase):
    def test_every_bank_from_its_record(self) -> None:
        disc = StubDisc()
        with tempfile.TemporaryDirectory() as tmp:
            paths = motion_banks.convert(disc, Path(tmp))
            got = {p.name: struct.unpack_from("<I", p.read_bytes(), 0x40)[0] for p in paths}
        self.assertEqual(len(got), 22)
        self.assertEqual(got["divmot00.bin"], 280)
        self.assertEqual(got["divmot15.bin"], 194)
        self.assertEqual(got["divmot20.bin"], 300)
        self.assertEqual(got["divmot99.bin"], 279)

    def test_rejects_a_record_that_is_no_bank(self) -> None:
        disc = StubDisc()
        with mock.patch.object(disc, "bns", lambda _id: b"\x01" + bytes(0x43)), \
                tempfile.TemporaryDirectory() as tmp, self.assertRaises(motion.MotionError):
            motion_banks.convert(disc, Path(tmp))


class RigsTest(unittest.TestCase):
    def test_joints_by_slot_with_z_negated(self) -> None:
        rng = random.Random(3)
        for _ in range(20):
            model, parents, offsets = _random_model(rng)
            scale, got_parents, got_offsets = rigs.rig(model)
            self.assertEqual(scale, model["scale_percent"] * 4096 // 100)
            self.assertEqual(got_parents, [parents[j] for j in range(rigs.JOINTS)])
            self.assertEqual(got_offsets, [[x, y, -z] for x, y, z in (offsets[j] for j in range(rigs.JOINTS))])

    def test_scale_rounds_down(self) -> None:
        model, _, _ = _random_model(random.Random(1))
        model["scale_percent"] = 105
        self.assertEqual(rigs.rig(model)[0], 4300)

    def test_rejects_missing_twice_given_and_late_parents(self) -> None:
        model, parents, offsets = _random_model(random.Random(2))
        missing = dict(model, parts=model["parts"][1:])
        twice = dict(model, parts=model["parts"] + model["parts"][:1])
        late = _model("late", 100, list(range(rigs.JOINTS)), {**parents, 4: 9}, offsets)
        for bad in (missing, twice, late):
            with self.subTest(parts=len(bad["parts"])), self.assertRaises(ValueError):
                rigs.rig(bad)

    def test_writes_the_costumes_in_the_file_layout(self) -> None:
        rng = random.Random(4)
        with tempfile.TemporaryDirectory() as tmp:
            imported, out = Path(tmp, "imported"), Path(tmp, "out")
            models = {}
            for name in ("costume_00", "costume_07", "enbu0_1"):
                models[name], _, _ = _random_model(rng, name)
                (imported / "characters" / name).mkdir(parents=True)
                (imported / "characters" / name / "model.json").write_text(json.dumps(models[name]))
            paths = rigs.convert(imported, out)
            self.assertEqual([p.name for p in paths], ["costume_00.bin", "costume_07.bin"])
            data = paths[1].read_bytes()
        scale, parents, offsets = rigs.rig(models["costume_07"])
        self.assertEqual(len(data), 12 + 16 * rigs.JOINTS)
        self.assertEqual(data[:12], b"T3RG" + struct.pack("<Ii", 1, scale))
        for j in range(rigs.JOINTS):
            self.assertEqual(struct.unpack_from("<b3x3i", data, 12 + 16 * j), (parents[j], *offsets[j]))


class CharactersTest(unittest.TestCase):
    def test_slot_and_clamped_bank_per_key(self) -> None:
        fight = {"characters": [{"bank": b, "name": n} for b, n in ((0, "A"), (9, "B"), (25, "LONG NAME HERE!"))]}
        fighter = {"costume_keys": [0, -1, 7]}
        rows = characters.entries(fight, fighter)
        self.assertEqual(rows, [(0, 0, "A"), (-1, 9, "B"), (7, 20, "LONG NAME HERE!")])
        data = characters.encode(rows)
        self.assertEqual(data[:12], b"T3CH" + struct.pack("<II", 1, 3))
        self.assertEqual(len(data), 12 + 16 * 3)
        self.assertEqual(struct.unpack_from("<bB14s", data, 12 + 16 * 2), (7, 20, b"LONG NAME HER\0"))

    def test_keys_past_the_records_take_record_0x58(self) -> None:
        fight = {"characters": [{"bank": k % 21, "name": str(k)} for k in range(0x5D)]}
        rows = characters.entries(fight, {"costume_keys": [0] * 0x5F})
        self.assertEqual(rows[0x5C][2], "92")
        self.assertEqual(rows[0x5D][2], "88")


class ModelsTest(unittest.TestCase):
    def test_texture_side_is_a_power_of_two_inside_the_page(self) -> None:
        self.assertEqual(models.texture_side(10, 12), (10, 8))
        self.assertEqual(models.texture_side(0, 63), (0, 64))
        self.assertEqual(models.texture_side(64, 65), (64, 8))
        self.assertEqual(models.texture_side(250, 255), (248, 8))
        self.assertEqual(models.texture_side(130, 200), (128, 128))
        self.assertEqual(models.texture_side(0, 255), (0, 256))

    def test_material_crops_its_rectangle(self) -> None:
        page = np.arange(256 * 256).reshape(256, 256) % 16
        clut = [0x7FFF - i for i in range(16)]
        m = models.material_texture(page, clut, 4, [(20, 40), (30, 47)])
        self.assertEqual((m.u0, m.v0, m.width, m.height), (20, 40, 16, 8))
        self.assertTrue((m.texels == page[40:48, 20:36]).all())
        self.assertEqual(m.palette, clut)
        self.assertFalse(m.transparent)

    def test_transparent_colours_move_to_index_0(self) -> None:
        page = np.tile(np.arange(16), (8, 1))
        clut = [0x1234, 1, 2, 0, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 0, 0x8000]
        m = models.material_texture(page, clut, 4, [(0, 0), (15, 7)])
        self.assertTrue(m.transparent)
        # 3 and 14 read 0; the opaque colour of index 0 moved to 3; 0x8000 (opaque black) stays.
        self.assertEqual(m.palette[0], 0)
        self.assertEqual(m.palette[3], 0x1234)
        self.assertEqual(m.palette[15], 0)
        self.assertEqual(list(m.texels[0]), [3, 1, 2, 0, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 0, 15])
        shown = [m.palette[i] for i in m.texels[0]]
        want = [c & 0x7FFF for c in clut]
        self.assertEqual(shown, want)

    def test_transparent_index_0_alone_stays(self) -> None:
        page = np.zeros((8, 8), dtype=int)
        m = models.material_texture(page, [0] + [5] * 15, 4, [(0, 0), (7, 7)])
        self.assertTrue(m.transparent)
        self.assertEqual(m.palette, [0] + [5] * 15)

    def test_encoding_packs_4_bit_texels_low_nibble_first(self) -> None:
        texels = np.array([[1, 2] * 4] * 8)
        m = models.Material(4, 8, 8, 0, 0, [7] * 16, texels, True)
        data = m.encode()
        self.assertEqual(struct.unpack_from("<BBHHHI", data), (4, 1, 8, 8, 16, 32))
        self.assertEqual(data[12 + 32], 0x21)
        self.assertEqual(len(data), 12 + 32 + 32)

    def test_model_layout(self) -> None:
        model = models.Model(vertices=[(0, 1, 2, 3), (11, -4, 5, -6)], normals=[(1, 0, 4096, 0)],
                             materials=[models.Material(8, 8, 8, 0, 0, list(range(256)), np.zeros((8, 8)), False)],
                             batches=[(0, 4, 1, 0, 1)], corners=[(0, 1, 0, 128, 3, 4)] * 4)
        data = models.encode(model)
        self.assertEqual(data[:20], b"T3MD" + struct.pack("<IHHHHI", 1, 2, 1, 1, 1, 4))
        self.assertEqual(struct.unpack_from("<Bx3h", data, 28), (11, -4, 5, -6))
        material_end = 44 + 12 + 512 + 64
        self.assertEqual(struct.unpack_from("<HBBII", data, material_end), (0, 4, 1, 0, 1))
        self.assertEqual(struct.unpack_from("<HHHBBBx", data, material_end + 12), (0, 1, 0, 128, 3, 4))
        self.assertEqual(len(data), material_end + 12 + 4 * 10)


class ConvertTest(unittest.TestCase):
    def test_image_required(self) -> None:
        with mock.patch("sys.argv", ["convert"]), mock.patch("sys.stderr"), \
                self.assertRaises(SystemExit) as exit_info:
            convert.main()
        self.assertEqual(exit_info.exception.code, 2)


if __name__ == "__main__":
    unittest.main()
