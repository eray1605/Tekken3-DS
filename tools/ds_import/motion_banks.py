"""The motion banks for the DS: each bank's BNS record as the disc has it, `motion/<bank>.bin`
(divmot00–divmot20 and the common bank divmot99, tools/remake_import/motion_bake.BANKS).

The DS reads the banks in place and samples their animation streams at run time
(ds/source/core/fighter/motion_bank.c, anim_stream.arm.c); the remake's baked poses would not fit.
"""

from __future__ import annotations

from pathlib import Path

import motion
import motion_bake

TARGET_DIR = "motion"


def convert(disc, out: Path) -> list[Path]:
    """`disc`: a common.Disc in Japan Rev.1's layout (its `bns(record_id)`)."""
    paths = []
    for name, record_id in motion_bake.BANKS:
        data = disc.bns(record_id)
        motion.parse_bank(data)   # the header and the section directory, as motion_bank_parse checks them
        target = out / TARGET_DIR / f"{name}.bin"
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
        paths.append(target)
    return paths
