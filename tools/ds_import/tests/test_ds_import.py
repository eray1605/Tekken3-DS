"""The DS converter's modules on a stub disc and synthetic models."""

from __future__ import annotations

import json
import random
import struct
import tempfile
import unittest
from pathlib import Path
from unittest import mock

import characters
import convert
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


class ConvertTest(unittest.TestCase):
    def test_image_required(self) -> None:
        with mock.patch("sys.argv", ["convert"]), mock.patch("sys.stderr"), \
                self.assertRaises(SystemExit) as exit_info:
            convert.main()
        self.assertEqual(exit_info.exception.code, 2)


if __name__ == "__main__":
    unittest.main()
