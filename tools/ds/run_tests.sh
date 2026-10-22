#!/bin/sh
# Builds the DS core for the host and runs its tests (ds/tests/): the math helpers, the pose solver,
# animation sampling and the fighter skeleton against the verified Python ports on synthetic tables
# (tools/ds/core_cases.py, no game data), and, when they exist, against the golden traces with the
# converted tables (ds/nitrofs/, work/traces/).
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

# The cases are made again when their generator or a port it runs changed.
CASES="$OUT/skeleton_cases.bin"
for source in tools/ds/core_cases.py tools/ds_import/pose_tables.py tools/research/pose.py \
        tools/research/motion.py tools/research/fight_sim.py tools/research/fight_math.py; do
    if [ ! -f "$CASES" ] || [ "$ROOT/$source" -nt "$CASES" ]; then
        python3 "$ROOT/tools/ds/core_cases.py" "$OUT"
        break
    fi
done

cd "$ROOT/ds"
exec "$OUT/t3ds_tests" --cases "$OUT" --nitrofs "$ROOT/ds/nitrofs" --traces "$ROOT/work/traces" "$@"
