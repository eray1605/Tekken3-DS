"""The fighters' identities for the DS: per costume key (character · 4 + costume) the costume slot
(its model and rig) and the motion bank type, from `tables/fighter.json` (costume_keys) and
`tables/fight.json` (characters) into `tables/characters.bin` (read by
ds/source/content/characters_file.c). As FightSimulation._identity: keys above 0x5C take record
0x58, and the bank type is clamped to the last one (Tekken Force's enemies, 20).

    char[4] "T3CH", u32 version, u32 key count, per key: s8 costume slot (−1: none), u8 bank type,
    char name[14] (the record's name, zero-padded); little-endian.
"""

from __future__ import annotations

import json
import struct
from pathlib import Path

MAGIC = b"T3CH"
VERSION = 1
NAME_BYTES = 14
LAST_BANK_TYPE = 20
TARGET = "tables/characters.bin"


def entries(fight: dict, fighter: dict) -> list[tuple[int, int, str]]:
    """(costume slot, bank type, name) per costume key of costume_keys."""
    records = fight["characters"]
    out = []
    for key, slot in enumerate(fighter["costume_keys"]):
        record = records[min(min(key, 0x58) if key > 0x5C else key, len(records) - 1)]
        out.append((slot, min(record["bank"], LAST_BANK_TYPE), record["name"]))
    return out


def encode(rows: list[tuple[int, int, str]]) -> bytes:
    out = bytearray(MAGIC + struct.pack("<II", VERSION, len(rows)))
    for slot, bank, name in rows:
        if not -1 <= slot < 128 or not 0 <= bank < 256:
            raise ValueError(f"characters: slot {slot} or bank {bank} out of range")
        out += struct.pack(f"<bB{NAME_BYTES}s", slot, bank, name.encode("ascii", "replace")[:NAME_BYTES - 1])
    return bytes(out)


def convert(imported: Path, out: Path) -> list[Path]:
    fight = json.loads((imported / "tables" / "fight.json").read_text())
    fighter = json.loads((imported / "tables" / "fighter.json").read_text())
    target = out / TARGET
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_bytes(encode(entries(fight, fighter)))
    return [target]
