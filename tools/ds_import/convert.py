#!/usr/bin/env python3
"""Convert the remake's converted tree into the DS build's data (docs/ds-port-plan.md).

Reads `remake/imported/` (made from the user's disc by tools/remake_import/convert.py) and, for
the raw data the remake bakes away (the motion banks' streams, the spline tables), the disc image
itself (tools/remake_import/common.Disc, any supported release rebased onto Japan Rev.1's layout).
Writes the DS formats into `ds/nitrofs/`, which the ROM build packs into NitroFS. Both folders hold
game data: they are git-ignored and must never be published.

    python3 tools/ds_import/convert.py --image <disc>.cue [--imported DIR] [--out DIR]
"""

from __future__ import annotations

import argparse
import json
import logging
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(1, str(ROOT / "tools" / "remake_import"))

import characters  # noqa: E402
import common  # noqa: E402  (puts tools/research on the import path too)
import motion_banks  # noqa: E402
import pose_tables  # noqa: E402
import rigs  # noqa: E402
import spline_tables  # noqa: E402

DEFAULT_IMPORTED = ROOT / "remake" / "imported"
DEFAULT_OUT = ROOT / "ds" / "nitrofs"
# What the DS build uses so far (D1: one fighter): from the converted tree, and from the disc.
MODULES = [pose_tables, rigs, characters]
DISC_MODULES = [motion_banks, spline_tables]

log = logging.getLogger("ds_import")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--image", type=Path, required=True,
                        help="the disc image (a cue sheet, CHD, ...) remake/imported was converted from")
    parser.add_argument("--imported", type=Path, default=DEFAULT_IMPORTED, help="the remake's converted tree")
    parser.add_argument("--out", type=Path, default=DEFAULT_OUT, help="the DS data folder")
    args = parser.parse_args()
    logging.basicConfig(level=logging.INFO, format="%(message)s")
    if not (args.imported / "manifest.json").is_file():
        log.error("%s is not a converted tree: run `make convert` first", args.imported)
        return 1
    try:
        disc = common.Disc.open(args.image)
    except common.DiscError as e:
        log.error("%s", e)
        return 1
    source = json.loads((args.imported / "manifest.json").read_text()).get("source")
    if source != disc.release.key:
        log.warning("%s is the %s release, %s was converted from %s", args.image.name, disc.release.label,
                    args.imported, source)
    disc = disc.canonical()
    paths = [p for m in MODULES for p in m.convert(args.imported, args.out)]
    paths += [p for m in DISC_MODULES for p in m.convert(disc, args.out)]
    for path in paths:
        log.info("%s (%d bytes)", path.relative_to(args.out), path.stat().st_size)
    log.info("%d files, %d bytes in %s", len(paths), sum(p.stat().st_size for p in paths), args.out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
