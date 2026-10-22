// The libgte routines the fighter code calls, with the GTE's integer rules
// (remake/core/math/gte.gd, tools/research/projection_sim.py).
#ifndef T3DS_CORE_MATH_GTE_H
#define T3DS_CORE_MATH_GTE_H

#include "core/math/fx.h"

// ApplyMatrix / ApplyRotMatrix: (m · v) >> 12 per row, read from the 32-bit MAC registers.
Vec3 gte_apply(const Mat3 *m, int64_t x, int64_t y, int64_t z);

// ApplyMatrixLV / ApplyRotMatrixLV: m · v for 32-bit components, as the high parts (>> 15,
// unshifted product, then << 3) plus the low 15 bits (product >> 12), signs kept.
Vec3 gte_apply_lv(const Mat3 *m, const Vec3 *v);

// VectorNormal (0x8003A86C): the vector (loaded as 16-bit) scaled to about 4096 with the
// reciprocal square root table (0xC0 entries, indexed from 0x40).
Vec3 gte_vector_normal(const Vec3 *v, const int16_t *rsqrt);

// libgte ratan2(y, x): the angle of (x, y) in 4096 units from a 1,025-entry octant table.
int32_t gte_ratan2(int64_t y, int64_t x, const int16_t *table);

// RTPS of (0, 0, 0) with the translation tr (sf = 1, lm = 0): SX, SY (clamped to −0x400…0x3FF)
// and SZ3 for projection plane h and the screen offsets ofx, ofy (pixels).
Vec3 gte_rtps_origin(const Vec3 *tr, int32_t h, int32_t ofx, int32_t ofy);

// The GTE's perspective division H / SZ3 (0.16 fixed point, at most 0x1FFFF) by the unsigned
// Newton–Raphson reciprocal.
int32_t gte_divide(int64_t h, int64_t sz3);

#endif
