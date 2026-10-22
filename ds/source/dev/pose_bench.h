// The pose solver's cost on the DS, before game data is converted: synthetic tables of the game's
// shape and pseudo-random pose vectors, timed with a hardware timer. Results tell the budget for
// docs/ds-port-plan.md (D1); the solver's correctness is proven on the host (tools/ds/run_tests.sh).
#ifndef T3DS_DEV_POSE_BENCH_H
#define T3DS_DEV_POSE_BENCH_H

#include <stdint.h>

typedef struct {
    uint32_t calls;
    uint32_t best_us;   // fastest call
    uint32_t mean_us;
    uint32_t worst_us;
} PoseBenchResult;

// Builds the synthetic tables (once) and times `calls` pose builds on one fighter's slots.
PoseBenchResult pose_bench_run(uint32_t calls);

#endif
