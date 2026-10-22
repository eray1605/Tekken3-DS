// Constant tables of the pose pipeline, converted from the game executable (remake/content/
// pose_tables.gd; on the DS `tables/pose.bin`, content/pose_tables_file.c): sine/cosine,
// SquareRoot0 mantissas, the Euler joint slots, the limb basis and the normalised skeleton offsets.
#ifndef T3DS_CORE_FIGHTER_POSE_TABLES_H
#define T3DS_CORE_FIGHTER_POSE_TABLES_H

#include <stdint.h>

#include "core/math/fx.h"

#define POSE_SIN_ENTRIES (4096 + 1024)  // g_sinTable; g_cosTable starts a quarter turn in
#define POSE_COS_OFFSET 1024
#define POSE_SQRT_BASE 0x40
#define POSE_SQRT_ENTRIES (0x100 - POSE_SQRT_BASE)
#define POSE_EULER_JOINTS 10

typedef struct {
    int16_t sin[POSE_SIN_ENTRIES];
    int16_t sqrt[POSE_SQRT_ENTRIES];
    uint8_t euler_slots[POSE_EULER_JOINTS];
    Mat3 limb_basis;
    Vec3 skeleton_offsets[4];  // chest, right/left shoulder, hips
    Vec3 limb_offsets[4];      // IK root offsets: arms, legs
} PoseTables;

// Sine of a 16-bit angle (65536 = one turn), 4.12, as `tab.sin` in the pose code.
static inline int32_t pose_sin16(const PoseTables *t, int32_t angle)
{
    return t->sin[(angle >> 4) & 0xFFF];
}

static inline int32_t pose_cos16(const PoseTables *t, int32_t angle)
{
    return t->sin[POSE_COS_OFFSET + ((angle >> 4) & 0xFFF)];
}

// Entry of the 4096-unit tables (`fight_math.trig_raw`).
static inline int32_t pose_sin12(const PoseTables *t, int32_t index)
{
    return t->sin[index & 0xFFF];
}

static inline int32_t pose_cos12(const PoseTables *t, int32_t index)
{
    return t->sin[POSE_COS_OFFSET + (index & 0xFFF)];
}

// libgte SquareRoot0 with the game's mantissa table. Defined for 0 <= a < 2^31: the pose code's
// sums stay there (a wrapped, negative sum would read before the table in the game).
int32_t pose_square_root0(const PoseTables *t, int32_t a);

#endif
