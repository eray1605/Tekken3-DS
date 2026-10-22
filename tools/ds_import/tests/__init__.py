"""The DS converter's tests (unittest, no game data): part of `make test-ds`."""

import sys
from pathlib import Path

sys.path.insert(1, str(Path(__file__).resolve().parents[2] / "remake_import"))
import common  # noqa: E402,F401  (puts tools/research on the import path)
