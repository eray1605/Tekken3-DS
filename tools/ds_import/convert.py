#!/usr/bin/env python3
"""Convert the remake's converted tree into the DS build's data (docs/ds-port-plan.md).

Reads `remake/imported/` (made from the user's disc by tools/remake_import/convert.py) and writes
the DS formats into `ds/nitrofs/`, which the ROM build packs into NitroFS. Both folders hold game
data: they are git-ignored and must never be published.

    python3 tools/ds_import/convert.py [--imported DIR] [--out DIR]
"""

from __future__ import annotations

import argparse
import logging
import sys
from pathlib import Path

import pose_tables

ROOT = Path(__file__).resolve().parents[2]
DEFAULT_IMPORTED = ROOT / "remake" / "imported"
DEFAULT_OUT = ROOT / "ds" / "nitrofs"
# What the DS build uses so far (D1: one fighter).
MODULES = [pose_tables]

log = logging.getLogger("ds_import")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--imported", type=Path, default=DEFAULT_IMPORTED, help="the remake's converted tree")
    parser.add_argument("--out", type=Path, default=DEFAULT_OUT, help="the DS data folder")
    args = parser.parse_args()
    logging.basicConfig(level=logging.INFO, format="%(message)s")
    if not (args.imported / "manifest.json").is_file():
        log.error("%s is not a converted tree: run `make convert` first", args.imported)
        return 1
    for module in MODULES:
        for path in module.convert(args.imported, args.out):
            log.info("%s (%d bytes)", path.relative_to(args.out), path.stat().st_size)
    return 0


if __name__ == "__main__":
    sys.exit(main())
