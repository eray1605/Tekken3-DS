// The integer helpers reproduce the R3000 and GTE behaviour the ports rely on
// (remake/tests/core/test_fx.gd).
#include "core/fighter/joint_frame.h"
#include "core/math/c_rand.h"
#include "core/math/fx.h"
#include "core/math/gte.h"
#include "test.h"

static bool mat_equal(const Mat3 *a, const Mat3 *b)
{
    for (int i = 0; i < 9; i++)
        if (a->m[i] != b->m[i])
            return false;
    return true;
}

void test_fx_s16_wraps(void)
{
    EXPECT_EQ(fx_s16(0x7FFF), 0x7FFF);
    EXPECT_EQ(fx_s16(0x8000), -0x8000);
    EXPECT_EQ(fx_s16(0x12345), 0x2345);
    EXPECT_EQ(fx_s16(-1), -1);
}

void test_fx_sat16_saturates(void)
{
    EXPECT_EQ(fx_sat16(40000), 0x7FFF);
    EXPECT_EQ(fx_sat16(-40000), -0x8000);
    EXPECT_EQ(fx_sat16(123), 123);
}

void test_fx_w32_wraps_like_c_int(void)
{
    EXPECT_EQ(fx_w32(0x7FFFFFFFLL + 1), -0x80000000LL);
    EXPECT_EQ(fx_w32(0x100000005LL), 5);
    EXPECT_EQ(fx_w32(-0x80000001LL), 0x7FFFFFFF);
    EXPECT_EQ(fx_mul32(0x10000, 0x10000), 0);
    EXPECT_EQ(fx_mul32(-3, 0x40000000), 0x40000000);
}

void test_fx_division_and_shifts_round_like_c(void)
{
    EXPECT_EQ(fx_div_trunc(-7, 2), -3);
    EXPECT_EQ(fx_div_trunc(7, -2), -3);
    EXPECT_EQ(fx_trunc12(-1), 0);
    EXPECT_EQ(fx_trunc12(-4097), -1);
    int64_t n = -4097;
    EXPECT(n >> 12 == -2, "arithmetic shift floors, as MIPS sra");
    EXPECT_EQ(fx_bit_length(0), 0);
    EXPECT_EQ(fx_bit_length(-5), 0);
    EXPECT_EQ(fx_bit_length(0x7FFFFFFF), 31);
}

void test_fx_mul_matrix_identity(void)
{
    Mat3 m = {{100, -200, 300, 4096, 0, -4096, 7, 8, 9}};
    Mat3 left = fx_mul_matrix(&FX_IDENTITY, &m);
    Mat3 right = fx_mul_matrix(&m, &FX_IDENTITY);
    EXPECT(mat_equal(&left, &m), "identity × m");
    EXPECT(mat_equal(&right, &m), "m × identity");
}

void test_compose_offset_uses_parent_rotation(void)
{
    // 90° about z: x → y.
    JointFrame parent = {{{0, -FX_ONE, 0, FX_ONE, 0, 0, 0, 0, FX_ONE}}, {{10, 20, 30}}};
    Vec3 offset = {{100, 0, 0}};
    JointFrame child = joint_frame_compose_offset(&parent, &FX_IDENTITY, &offset);
    EXPECT_EQ(child.t.v[0], 10);
    EXPECT_EQ(child.t.v[1], 120);
    EXPECT_EQ(child.t.v[2], 30);
    EXPECT(mat_equal(&child.rot, &parent.rot), "rotation kept");
}

void test_c_rand_sequence(void)
{
    // x = x · 0x41C64E6D + 0x3039 from 1: 0x41C67EA6, 0x967EB0E7, …; the result is bits 16–30.
    CRand r;
    c_rand_seed(&r, 1);
    EXPECT_EQ(c_rand_next(&r), 0x41C6);
    EXPECT_EQ(r.state, 0x41C67EA6u);
    EXPECT_EQ(c_rand_next(&r), 0x167E);
    EXPECT_EQ(r.state, 0x967EB0E7u);
}

void test_gte_divide_and_ratan2(void)
{
    // H / SZ3 in 0.16: exact quotients and the overflow limit.
    EXPECT_EQ(gte_divide(0x100, 0x200), 0x8000);
    EXPECT_EQ(gte_divide(0x400, 0x200), 0x1FFFF);
    EXPECT_EQ(gte_divide(0x300, 0x400), 0xC000);
    // ratan2's octant logic with a linear table (tangent t / 1024 → t / 2, 0x200 at 45°): the
    // quadrants and the axes.
    static int16_t table[1025];
    for (int i = 0; i < 1025; i++)
        table[i] = (int16_t)(i / 2);
    EXPECT_EQ(gte_ratan2(0, 0, table), 0);
    EXPECT_EQ(gte_ratan2(0, 100, table), 0);
    EXPECT_EQ(gte_ratan2(100, 100, table), 0x200);
    EXPECT_EQ(gte_ratan2(100, 0, table), 0x400);
    EXPECT_EQ(gte_ratan2(0, -100, table), 0x800);
    EXPECT_EQ(gte_ratan2(-100, 100, table), -0x200);
    EXPECT_EQ(gte_ratan2(50, 100, table), 0x100);
}
