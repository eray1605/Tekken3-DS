#include "dev/pose_bench.h"

#include <nds.h>

#include "core/fighter/pose_solver.h"
#include "core/math/c_rand.h"

#define TIMER 0

static PoseTables tables;
static bool tables_ready;

static uint32_t isqrt(uint32_t v)
{
    uint32_t root = 0;
    for (uint32_t bit = 1u << 30; bit; bit >>= 2) {
        if (v >= root + bit) {
            v -= root + bit;
            root = (root >> 1) + bit;
        } else {
            root >>= 1;
        }
    }
    return root;
}

// The same construction as tools/ds/pose_cases.py, with libnds's sine instead of Python's.
static void build_tables(void)
{
    for (int i = 0; i < POSE_SIN_ENTRIES; i++)
        tables.sin[i] = sinLerp((int16_t)(i << 3));
    for (int x = POSE_SQRT_BASE; x < 0x100; x++)
        tables.sqrt[x - POSE_SQRT_BASE] = (int16_t)isqrt((uint32_t)x << 18);
    static const uint8_t EULER_SLOTS[POSE_EULER_JOINTS] = {0, 1, 2, 3, 6, 7, 10, 11, 14, 17};
    for (int j = 0; j < POSE_EULER_JOINTS; j++)
        tables.euler_slots[j] = EULER_SLOTS[j];
    tables.limb_basis = pose_euler_to_matrix(&tables, 0x1234, -0x2345, 0x3456);
    static const Vec3 SKELETON[4] = {{{0, -220, 0}}, {{-140, -40, 0}}, {{140, -40, 0}}, {{0, 30, 0}}};
    static const Vec3 LIMB[4] = {{{20, 0, -10}}, {{-20, 0, -10}}, {{10, 40, 0}}, {{-10, 40, 0}}};
    for (int i = 0; i < 4; i++) {
        tables.skeleton_offsets[i] = SKELETON[i];
        tables.limb_offsets[i] = LIMB[i];
    }
    tables_ready = true;
}

static int32_t random_range(CRand *rng, int32_t span)
{
    return c_rand_next(rng) % (2 * span + 1) - span;
}

static void random_pose(CRand *rng, int32_t pose[POSE_CHANNELS])
{
    for (int k = 0; k < 3; k++)
        pose[k] = random_range(rng, 1500);
    for (int c = 3; c < 15; c++)
        pose[c] = pose[c % 3] + random_range(rng, 600);
    for (int c = 15; c < POSE_CHANNELS; c++)
        pose[c] = (int16_t)(c_rand_next(rng) << 1);
}

PoseBenchResult pose_bench_run(uint32_t calls)
{
    if (!tables_ready)
        build_tables();
    PoseSolver solver = {&tables, false};
    Mat3 slots[POSE_SLOT_COUNT];
    for (int s = 0; s < POSE_SLOT_COUNT; s++)
        slots[s] = FX_IDENTITY;
    CRand rng;
    c_rand_seed(&rng, 1);
    PoseBenchResult result = {calls, UINT32_MAX, 0, 0};
    uint64_t total = 0;
    for (uint32_t i = 0; i < calls; i++) {
        int32_t pose[POSE_CHANNELS];
        random_pose(&rng, pose);
        cpuStartTiming(TIMER);
        pose_solver_build(&solver, pose, slots);
        uint32_t us = timerTicks2usec(cpuEndTiming());
        total += us;
        if (us < result.best_us)
            result.best_us = us;
        if (us > result.worst_us)
            result.worst_us = us;
    }
    result.mean_us = calls ? (uint32_t)(total / calls) : 0;
    return result;
}
