#!/bin/sh
# Builds the DS core for the host and runs its tests (ds/tests/): the math helpers, the pose solver
# against the verified Python port on synthetic tables (tools/ds/pose_cases.py, no game data), and,
# when they exist, against the golden traces with the converted tables (ds/nitrofs/, work/traces/).
#   tools/ds/run_tests.sh [filter]
# CC picks the host compiler (default cc).
set -eu
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
OUT="$ROOT/ds/build/host"
CC=${CC:-cc}
mkdir -p "$OUT"

SOURCES=$(find "$ROOT/ds/source/core" "$ROOT/ds/source/content" "$ROOT/ds/tests" -name '*.c' | sort)
# shellcheck disable=SC2086
"$CC" -std=gnu17 -O2 -g -Wall -Wextra -Werror -fsanitize=undefined -fno-sanitize-recover=undefined \
    -I"$ROOT/ds/source" -I"$ROOT/ds/tests" $SOURCES -o "$OUT/t3ds_tests"

CASES="$OUT/pose_cases.bin"
if [ ! -f "$CASES" ] || [ "$ROOT/tools/ds/pose_cases.py" -nt "$CASES" ] || [ "$ROOT/tools/research/pose.py" -nt "$CASES" ]; then
    python3 "$ROOT/tools/ds/pose_cases.py" "$OUT"
fi

cd "$ROOT/ds"
exec "$OUT/t3ds_tests" --cases "$OUT" --nitrofs "$ROOT/ds/nitrofs" --traces "$ROOT/work/traces" "$@"
