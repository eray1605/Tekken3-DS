#include "core/math/fx.h"

const Mat3 FX_IDENTITY = {{FX_ONE, 0, 0, 0, FX_ONE, 0, 0, 0, FX_ONE}};

Vec3 fx_mvmva(const Mat3 *m, int64_t x, int64_t y, int64_t z)
{
    const int32_t *r = m->m;
    return (Vec3){{
        fx_sat16((r[0] * x + r[1] * y + r[2] * z) >> 12),
        fx_sat16((r[3] * x + r[4] * y + r[5] * z) >> 12),
        fx_sat16((r[6] * x + r[7] * y + r[8] * z) >> 12),
    }};
}

Vec3 fx_mvmva_gpf(int64_t ir0, int64_t a, int64_t b, int64_t c)
{
    int64_t w = fx_s16(ir0);
    return (Vec3){{
        fx_sat16((w * fx_s16(a)) >> 12),
        fx_sat16((w * fx_s16(b)) >> 12),
        fx_sat16((w * fx_s16(c)) >> 12),
    }};
}

Mat3 fx_mul_matrix(const Mat3 *m0, const Mat3 *m1)
{
    Mat3 out;
    for (int c = 0; c < 3; c++) {
        Vec3 col = fx_mvmva(m0, m1->m[c], m1->m[3 + c], m1->m[6 + c]);
        out.m[c] = fx_s16(col.v[0]);
        out.m[3 + c] = fx_s16(col.v[1]);
        out.m[6 + c] = fx_s16(col.v[2]);
    }
    return out;
}

Mat3 fx_transpose(const Mat3 *m)
{
    const int32_t *r = m->m;
    return (Mat3){{r[0], r[3], r[6], r[1], r[4], r[7], r[2], r[5], r[8]}};
}
