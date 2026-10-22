#include "core/math/gte.h"

static inline int64_t clamp64(int64_t v, int64_t lo, int64_t hi)
{
    return v < lo ? lo : v > hi ? hi : v;
}

static inline int64_t abs64(int64_t v)
{
    return v < 0 ? -v : v;
}

Vec3 gte_apply(const Mat3 *m, int64_t x, int64_t y, int64_t z)
{
    const int32_t *r = m->m;
    x = fx_s16(x);
    y = fx_s16(y);
    z = fx_s16(z);
    return (Vec3){{
        fx_w32((r[0] * x + r[1] * y + r[2] * z) >> 12),
        fx_w32((r[3] * x + r[4] * y + r[5] * z) >> 12),
        fx_w32((r[6] * x + r[7] * y + r[8] * z) >> 12),
    }};
}

Vec3 gte_apply_lv(const Mat3 *m, const Vec3 *v)
{
    int64_t hi[3];
    int64_t lo[3];
    for (int k = 0; k < 3; k++) {
        int64_t c = v->v[k];
        if (c >= 0) {
            hi[k] = c >> 15;
            lo[k] = c & 0x7FFF;
        } else {
            hi[k] = -((-c) >> 15);
            lo[k] = -((-c) & 0x7FFF);
        }
    }
    Vec3 out;
    for (int r = 0; r < 3; r++) {
        const int32_t *row = m->m + 3 * r;
        int64_t h = row[0] * hi[0] + row[1] * hi[1] + row[2] * hi[2];
        int64_t l = (row[0] * lo[0] + row[1] * lo[1] + row[2] * lo[2]) >> 12;
        out.v[r] = fx_w32(l + h * 8);
    }
    return out;
}

Vec3 gte_vector_normal(const Vec3 *v, const int16_t *rsqrt)
{
    int64_t x = fx_s16(v->v[0]);
    int64_t y = fx_s16(v->v[1]);
    int64_t z = fx_s16(v->v[2]);
    int64_t sum = fx_w32(x * x + y * y + z * z);
    if (sum == 0)
        return (Vec3){{0, 0, 0}};
    int lz = 0;
    if (sum >= 0)
        lz = (32 - fx_bit_length(sum)) & 0x1E;
    int shift = (31 - lz) >> 1;
    int64_t index = lz >= 24 ? fx_w32(fx_shl(sum, lz - 24)) : sum >> (24 - lz);
    int64_t ir0 = rsqrt[index - 0x40];
    return (Vec3){{fx_w32(ir0 * x) >> shift, fx_w32(ir0 * y) >> shift, fx_w32(ir0 * z) >> shift}};
}

int32_t gte_ratan2(int64_t y, int64_t x, const int16_t *table)
{
    y = fx_w32(y);
    x = fx_w32(x);
    int neg_x = x < 0;
    int neg_y = y < 0;
    x = abs64(x);
    y = abs64(y);
    if (x == 0 && y == 0)
        return 0;
    int32_t v;
    if (y < x) {
        int64_t t = (y & 0x7FE00000) == 0 ? fx_div_trunc(fx_w32(y << 10), x) : fx_div_trunc(y, x >> 10);
        v = table[t];
    } else {
        int64_t t = (x & 0x7FE00000) == 0 ? fx_div_trunc(fx_w32(x << 10), y) : fx_div_trunc(x, y >> 10);
        v = 0x400 - table[t];
    }
    if (neg_x)
        v = 0x800 - v;
    if (neg_y)
        v = -v;
    return v;
}

Vec3 gte_rtps_origin(const Vec3 *tr, int32_t h, int32_t ofx, int32_t ofy)
{
    int64_t sz = clamp64(tr->v[2], 0, 0xFFFF);
    int64_t div = gte_divide(h & 0xFFFF, sz);
    int64_t sx = clamp64((div * fx_sat16(tr->v[0]) + fx_shl(ofx, 16)) >> 16, -0x400, 0x3FF);
    int64_t sy = clamp64((div * fx_sat16(tr->v[1]) + fx_shl(ofy, 16)) >> 16, -0x400, 0x3FF);
    return (Vec3){{(int32_t)sx, (int32_t)sy, (int32_t)sz}};
}

int32_t gte_divide(int64_t h, int64_t sz3)
{
    if (h >= sz3 * 2)
        return 0x1FFFF;
    int z = 16 - fx_bit_length(sz3);
    int64_t n = fx_shl(h, z);
    int64_t d = fx_shl(sz3, z);
    int64_t i = (d - 0x7FC0) >> 7;
    int64_t u = ((0x40000 / (i + 0x100) + 1) >> 1) - 0x101;
    u = (u > 0 ? u : 0) + 0x101;
    d = (0x2000080 - d * u) >> 8;
    d = (0x0000080 + d * u) >> 8;
    int64_t q = (n * d + 0x8000) >> 16;
    return (int32_t)(q < 0x1FFFF ? q : 0x1FFFF);
}
