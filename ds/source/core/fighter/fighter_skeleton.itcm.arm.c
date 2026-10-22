#include "core/fighter/fighter_skeleton.h"

void fighter_skeleton_init(FighterSkeleton *skeleton, const PoseTables *tables, const FighterRig *rig)
{
    skeleton->tables = tables;
    skeleton->rig = rig;
    skeleton->solver = (PoseSolver){tables, false};
    for (int i = 0; i < SKELETON_JOINTS; i++)
        skeleton->local_slots[i] = FX_IDENTITY;
    skeleton->root = (JointFrame){FX_IDENTITY, {{0, 0, 0}}};
    for (int i = 0; i < SKELETON_ALL_JOINTS; i++)
        skeleton->joints[i] = skeleton->root;
}

Vec3 fighter_skeleton_root_displacement(const FighterSkeleton *skeleton, const int32_t pose[POSE_CHANNELS])
{
    int64_t scale = skeleton->rig->scale;
    return (Vec3){{
        fx_s16((pose[0] * scale) >> 12),
        fx_s16((pose[1] * scale) >> 12),
        fx_s16((pose[2] * scale) >> 12),
    }};
}

void fighter_skeleton_update(FighterSkeleton *skeleton, const int32_t pose[POSE_CHANNELS], const Vec3 *anchor,
                             int32_t facing, int32_t heading)
{
    const PoseTables *tables = skeleton->tables;
    const FighterRig *rig = skeleton->rig;
    pose_solver_build(&skeleton->solver, pose, skeleton->local_slots);
    Vec3 d = fighter_skeleton_root_displacement(skeleton, pose);
    int32_t a = ((0x8000 - facing) >> 4) & 0xFFF;
    int64_t c = pose_cos12(tables, a);
    int64_t s = pose_sin12(tables, a);
    skeleton->root.rot = fighter_rotation_xyz(tables, 0, (heading - 0x8000) & 0xFFFF, 0);
    skeleton->root.t = (Vec3){{
        (int32_t)(anchor->v[0] + fx_trunc12(d.v[0] * c - d.v[2] * s)),
        anchor->v[1] + d.v[1],
        (int32_t)(anchor->v[2] + fx_trunc12(d.v[0] * s + d.v[2] * c)),
    }};
    for (int m = 0; m < SKELETON_ALL_JOINTS; m++) {
        const JointFrame *parent = rig->parents[m] < 0 ? &skeleton->root : &skeleton->joints[rig->parents[m]];
        const Mat3 *local = m < SKELETON_JOINTS ? &skeleton->local_slots[m] : &FX_IDENTITY;
        skeleton->joints[m] = joint_frame_compose_offset(parent, local, &rig->offsets[m]);
    }
}

Mat3 fighter_rotation_xyz(const PoseTables *tables, int32_t x, int32_t y, int32_t z)
{
    int64_t cx = pose_cos12(tables, x >> 4);
    int64_t sx = pose_sin12(tables, x >> 4);
    int64_t cy = pose_cos12(tables, y >> 4);
    int64_t sy = pose_sin12(tables, y >> 4);
    int64_t cz = pose_cos12(tables, z >> 4);
    int64_t sz = pose_sin12(tables, z >> 4);
    int64_t a1 = fx_sat16((sx * cz) >> 12);
    int64_t a2 = fx_sat16((sx * sz) >> 12);
    int64_t a3 = fx_sat16((sx * cy) >> 12);
    int64_t cxcz = (cx * cz) >> 12;
    int64_t b1 = fx_sat16((sy * a1) >> 12);
    int64_t b2 = fx_sat16((sy * a2) >> 12);
    int64_t b3 = fx_sat16((sy * cxcz) >> 12);
    int64_t cxsz = (cx * sz) >> 12;
    return (Mat3){{
        fx_s16(fx_sat16((cy * cz) >> 12)), fx_s16(fx_sat16((cy * -sz) >> 12)), fx_s16(sy),
        fx_s16(b1 + cxsz), fx_s16(cxcz - b2), fx_s16(-a3),
        fx_s16(a2 - b3), fx_s16(((cxsz * sy) >> 12) + a1), fx_s16(fx_sat16((cy * cx) >> 12)),
    }};
}
