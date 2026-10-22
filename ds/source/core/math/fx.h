// Integer arithmetic of the original game: 4.12 fixed point, 16-bit GTE registers and 32-bit C
// ints (remake/core/math/fx.gd). Matrices are row-major, vectors three ints.
//
// The GDScript core computes in 64-bit ints and wraps or saturates explicitly where the original
// does. This port keeps that split: sums of GTE products are 64-bit, wrapped C arithmetic is
// 32-bit modular (fx_mul32 and the unsigned forms), never signed overflow.
#ifndef T3DS_CORE_MATH_FX_H
#define T3DS_CORE_MATH_FX_H

#include <stdint.h>

#define FX_ONE 0x1000

typedef struct {
    int32_t m[9];
} Mat3;

typedef struct {
    int32_t v[3];
} Vec3;

extern const Mat3 FX_IDENTITY;

// Wraps to a signed 16-bit value (a halfword store).
static inline int32_t fx_s16(int64_t v)
{
    return (int16_t)(uint16_t)(uint64_t)v;
}

// Wraps to a signed 8-bit value (a byte store).
static inline int32_t fx_s8(int64_t v)
{
    return (int8_t)(uint8_t)(uint64_t)v;
}

// Wraps to a signed 32-bit value, as C int arithmetic on the R3000.
static inline int32_t fx_w32(int64_t v)
{
    return (int32_t)(uint32_t)(uint64_t)v;
}

// a · b wrapped to 32 bits, as the R3000's mult keeping LO.
static inline int32_t fx_mul32(int32_t a, int32_t b)
{
    return (int32_t)((uint32_t)a * (uint32_t)b);
}

// GTE IR saturation with lm = 0.
static inline int32_t fx_sat16(int64_t v)
{
    return v < -0x8000 ? -0x8000 : v > 0x7FFF ? 0x7FFF : (int32_t)v;
}

// The number of bits needed for a value; 0 for zero and negative values (fx.gd's loop).
static inline int fx_bit_length(int64_t v)
{
    if (v <= 0)
        return 0;
    return 64 - __builtin_clzll((uint64_t)v);
}

// C integer division, truncating toward zero.
static inline int64_t fx_div_trunc(int64_t a, int64_t b)
{
    return a / b;
}

// Arithmetic shift right by 12 rounding toward zero (C division by 4096).
static inline int64_t fx_trunc12(int64_t v)
{
    return v < 0 ? (v + 0xFFF) >> 12 : v >> 12;
}

// v << n without signed-overflow undefined behaviour (64-bit, wraps like a two's complement shift).
static inline int64_t fx_shl(int64_t v, int n)
{
    return (int64_t)((uint64_t)v << n);
}

// GTE MVMVA rotation × vector with sf = 1, lm = 0 (IR results, saturated).
Vec3 fx_mvmva(const Mat3 *m, int64_t x, int64_t y, int64_t z);

// GTE GPF with sf = 1, lm = 0: sat16((ir0 · v) >> 12) for three IR values.
Vec3 fx_mvmva_gpf(int64_t ir0, int64_t a, int64_t b, int64_t c);

// libgte MulMatrix: m0 × m1, one column at a time through MVMVA, stored as s16.
Mat3 fx_mul_matrix(const Mat3 *m0, const Mat3 *m1);

Mat3 fx_transpose(const Mat3 *m);

#endif
