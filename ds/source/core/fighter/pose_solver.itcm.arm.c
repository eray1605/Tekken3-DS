#include "core/fighter/pose_solver.h"

#include "core/fighter/joint_frame.h"

#define EULER_CHANNEL 19
#define ARM_MAX_REACH 0x215
#define ARM_MIN_REACH 0x47
#define LEG_MAX_REACH 0x37B
#define LEG_MIN_REACH 3

static const int BASIS_SLOTS[] = {2, 3, 7};

enum LimbRoot { RIGHT_SHOULDER, LEFT_SHOULDER, HIPS };

// Limb solve order: IK root frame, target channel, offset index, swivel channel, upper and lower
// slot, arm or leg.
static const struct {
    enum LimbRoot root;
    int target_channel;
    int offset_index;
    int swivel_channel;
    int upper_slot;
    int lower_slot;
    bool is_arm;
} LIMBS[4] = {
    {RIGHT_SHOULDER, 3, 0, 15, 4, 5, true},
    {LEFT_SHOULDER, 6, 1, 16, 8, 9, true},
    {HIPS, 9, 2, 17, 12, 13, false},
    {HIPS, 12, 3, 18, 15, 16, false},
};

Mat3 pose_euler_to_matrix(const PoseTables *tables, int32_t x, int32_t y, int32_t z)
{
    int64_t sx = pose_sin16(tables, x);
    int64_t cx = pose_cos16(tables, x);
    int64_t sy = pose_sin16(tables, y);
    int64_t cy = pose_cos16(tables, y);
    int64_t sz = pose_sin16(tables, z);
    int64_t cz = pose_cos16(tables, z);
    int64_t a1 = fx_sat16((sx * cz) >> 12);
    int64_t a2 = fx_sat16((sx * sz) >> 12);
    int64_t a3 = fx_sat16((sx * cy) >> 12);
    int64_t cxcz = (cx * cz) >> 12;
    int64_t b1 = fx_sat16((sy * a1) >> 12);
    int64_t b2 = fx_sat16((sy * a2) >> 12);
    int64_t b3 = fx_sat16((sy * cxcz) >> 12);
    int64_t cxsz = (cx * sz) >> 12;
    return (Mat3){{
        fx_s16(fx_sat16((cy * cz) >> 12)),
        fx_s16(b1 - cxsz),
        fx_s16(a2 + b3),
        fx_s16(fx_sat16((cy * sz) >> 12)),
        fx_s16(cxcz + b2),
        fx_s16(((cxsz * sy) >> 12) - a1),
        fx_s16(-sy),
        fx_s16(a3),
        fx_s16(fx_sat16((cy * cx) >> 12)),
    }};
}

// IkLimbTarget: the target in the limb root's frame. The difference is rotated in two 15-bit
// halves (high·8 + low) for precision, as the game does through the GTE.
static Vec3 limb_target(const Vec3 *target, const JointFrame *root, const Vec3 *offset)
{
    Mat3 rt = fx_transpose(&root->rot);
    int64_t hi[3];
    int64_t lo[3];
    for (int i = 0; i < 3; i++) {
        int64_t d = (int64_t)target->v[i] - root->t.v[i];
        if (d < 0) {
            hi[i] = -((-d) >> 15);
            lo[i] = -((-d) & 0x7FFF);
        } else {
            hi[i] = d >> 15;
            lo[i] = d & 0x7FFF;
        }
    }
    Vec3 low = fx_mvmva(&rt, lo[0], lo[1], lo[2]);
    Vec3 out;
    for (int r = 0; r < 3; r++) {
        const int32_t *row = rt.m + 3 * r;
        int64_t high = fx_sat16(row[0] * hi[0] + row[1] * hi[1] + row[2] * hi[2]);
        out.v[r] = (int32_t)(low.v[r] + high * 8 + offset->v[r]);
    }
    return out;
}

// IkBendMatrix: rotates columns 0 and 1 of the upper limb by the bend angle.
static void bend(Mat3 *upper, int64_t s, int64_t c)
{
    int32_t *m = upper->m;
    int64_t a0 = m[0];
    int64_t a3 = m[3];
    int64_t a6 = m[6];
    m[0] = fx_s16((a0 * c + m[1] * s) >> 12);
    m[3] = fx_s16((a3 * c + m[4] * s) >> 12);
    m[1] = fx_s16((-a0 * s + m[1] * c) >> 12);
    m[4] = fx_s16((-a3 * s + m[4] * c) >> 12);
    m[6] = fx_s16((a6 * c + m[7] * s) >> 12);
    m[7] = fx_s16((-a6 * s + m[7] * c) >> 12);
}

// (a·b + c·d) wrapped to 32 bits: the original's int arithmetic.
static inline int32_t dot2_w32(int32_t a, int32_t b, int32_t c, int32_t d)
{
    return (int32_t)((uint32_t)a * (uint32_t)b + (uint32_t)c * (uint32_t)d);
}

// IkSolveLimb: aim rotation into `upper`, bend into `lower`. All products wrap to 32 bits like the
// original C code (game-bugs.md #8 relies on it for arm targets near 71–74).
static void solve_limb(const PoseSolver *solver, const Vec3 *target, int32_t swivel, bool is_arm,
                       Mat3 *upper_slot, Mat3 *lower_slot)
{
    const PoseTables *tables = solver->tables;
    int32_t *upper = upper_slot->m;
    int32_t x = target->v[0];
    int32_t y = target->v[1];
    int32_t z = target->v[2];
    int32_t sq = (int32_t)((uint32_t)x * (uint32_t)x + (uint32_t)y * (uint32_t)y + (uint32_t)z * (uint32_t)z);
    if (sq == 0)
        return;
    int32_t length = pose_square_root0(tables, sq);
    int32_t inv = (int32_t)fx_div_trunc(0x400000, length);
    int32_t ux = fx_mul32(x, inv) >> 10;
    int32_t uy = fx_mul32(y, inv) >> 10;
    int32_t uz = fx_mul32(z, inv) >> 10;
    int32_t k = ux + FX_ONE;
    int32_t a;
    int32_t e;
    if (k == 0) {
        k = -0x800;
        a = fx_mul32(y, inv) >> 11;
        e = -FX_ONE;
    } else {
        if (k < 0x2000) {
            k = (int32_t)fx_div_trunc(-0x1000000, k);
            a = fx_mul32(k, uy) >> 12;
        } else {
            k = -0x800;
            a = (-uy) >> 1;
        }
        e = FX_ONE;
    }
    int32_t b = (fx_mul32(a, uy) >> 12) + e;
    k = fx_mul32(k, uz) >> 12;
    upper[0] = fx_s16(ux);
    upper[3] = fx_s16(uy);
    upper[6] = fx_s16(uz);
    int32_t c = pose_cos16(tables, swivel);
    int32_t s = pose_sin16(tables, swivel);
    int32_t p = fx_mul32(a, e + ux) >> 12;
    int32_t q = fx_mul32(k, e + ux) >> 12;
    int32_t r = fx_mul32(k, uy) >> 12;
    int32_t w = fx_mul32(a, uz) >> 12;
    int32_t v = (fx_mul32(k, uz) >> 12) + FX_ONE;
    upper[1] = fx_s16(dot2_w32(c, p, s, q) >> 12);
    upper[4] = fx_s16(dot2_w32(c, b, s, r) >> 12);
    upper[7] = fx_s16(dot2_w32(c, w, s, v) >> 12);
    upper[2] = fx_s16(dot2_w32(-s, p, c, q) >> 12);
    upper[5] = fx_s16(dot2_w32(-s, b, c, r) >> 12);
    upper[8] = fx_s16(dot2_w32(-s, w, c, v) >> 12);
    int64_t d;
    int32_t cos_a;
    int32_t cos_b;
    if (is_arm) {
        if (length > ARM_MAX_REACH) {
            *lower_slot = FX_IDENTITY;
            return;
        }
        if (length < ARM_MIN_REACH)
            return;
        d = (solver->wide_elbow ? (int64_t)inv * 0x9204 : fx_mul32(inv, 0x9204)) >> 11;
        cos_a = fx_mul32((int32_t)((uint32_t)length * 0x800u + (uint32_t)d), 0xD9) >> 16;
        cos_b = fx_mul32((int32_t)((uint32_t)length * 0x800u - (uint32_t)d), 0x11A) >> 16;
    } else {
        if (length > LEG_MAX_REACH) {
            *lower_slot = FX_IDENTITY;
            return;
        }
        if (length < LEG_MIN_REACH)
            return;
        d = fx_mul32(inv, 0x6F8) >> 11;
        cos_a = fx_mul32((int32_t)((uint32_t)length * 0x800u + (uint32_t)d), 0x92) >> 16;
        cos_b = fx_mul32((int32_t)((uint32_t)length * 0x800u - (uint32_t)d), 0x93) >> 16;
    }
    int32_t sin_a = 0;
    if (cos_a > FX_ONE) {
        cos_a = FX_ONE;
    } else if (cos_a < -FX_ONE) {
        cos_a = -FX_ONE;
    } else {
        sin_a = pose_square_root0(tables, 0x1000000 - cos_a * cos_a);
        if (!is_arm)
            sin_a = -sin_a;
    }
    bend(upper_slot, fx_s16(sin_a), cos_a);
    int32_t sin_b = 0;
    if (cos_b > FX_ONE)
        cos_b = FX_ONE;
    else if (cos_b < -FX_ONE)
        cos_b = -FX_ONE;
    else
        sin_b = fx_mul32(sin_a, is_arm ? -0x14D4 : -0x1012) >> 12;
    int32_t diag = fx_s16(fx_trunc12(dot2_w32(cos_a, cos_b, sin_a, sin_b)));
    int32_t off = fx_s16(fx_trunc12(dot2_w32(-sin_a, cos_b, cos_a, sin_b)));
    *lower_slot = (Mat3){{diag, fx_s16(-off), 0, off, diag, 0, 0, 0, FX_ONE}};
}

void pose_solver_build(const PoseSolver *solver, const int32_t pose[POSE_CHANNELS], Mat3 slots[POSE_SLOT_COUNT])
{
    const PoseTables *tables = solver->tables;
    for (int j = 0; j < POSE_EULER_JOINTS; j++) {
        int c = EULER_CHANNEL + 3 * j;
        slots[tables->euler_slots[j]] = pose_euler_to_matrix(tables, pose[c], pose[c + 1], pose[c + 2]);
    }
    for (unsigned i = 0; i < sizeof(BASIS_SLOTS) / sizeof(BASIS_SLOTS[0]); i++) {
        int s = BASIS_SLOTS[i];
        slots[s] = fx_mul_matrix(&slots[s], &tables->limb_basis);
    }
    JointFrame root = {slots[0], {{pose[0], pose[1], pose[2]}}};
    JointFrame chest = joint_frame_compose_offset(&root, &slots[1], &tables->skeleton_offsets[0]);
    JointFrame frames[3] = {
        joint_frame_compose_offset(&chest, &slots[3], &tables->skeleton_offsets[1]),
        joint_frame_compose_offset(&chest, &slots[7], &tables->skeleton_offsets[2]),
        joint_frame_compose_offset(&root, &slots[11], &tables->skeleton_offsets[3]),
    };
    Vec3 targets[4];
    for (int i = 0; i < 4; i++) {
        int c = LIMBS[i].target_channel;
        Vec3 target = {{pose[c], pose[c + 1], pose[c + 2]}};
        targets[i] = limb_target(&target, &frames[LIMBS[i].root], &tables->limb_offsets[LIMBS[i].offset_index]);
    }
    for (int i = 0; i < 4; i++)
        solve_limb(solver, &targets[i], pose[LIMBS[i].swivel_channel], LIMBS[i].is_arm,
                   &slots[LIMBS[i].upper_slot], &slots[LIMBS[i].lower_slot]);
}
