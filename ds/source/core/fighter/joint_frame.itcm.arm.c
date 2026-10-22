#include "core/fighter/joint_frame.h"

JointFrame joint_frame_compose_offset(const JointFrame *parent, const Mat3 *local_rot, const Vec3 *offset)
{
    const int32_t *r = parent->rot.m;
    int64_t x = fx_s16(offset->v[0]);
    int64_t y = fx_s16(offset->v[1]);
    int64_t z = fx_s16(offset->v[2]);
    JointFrame out;
    out.t.v[0] = (int32_t)(parent->t.v[0] + ((r[0] * x + r[1] * y + r[2] * z) >> 12));
    out.t.v[1] = (int32_t)(parent->t.v[1] + ((r[3] * x + r[4] * y + r[5] * z) >> 12));
    out.t.v[2] = (int32_t)(parent->t.v[2] + ((r[6] * x + r[7] * y + r[8] * z) >> 12));
    out.rot = fx_mul_matrix(&parent->rot, local_rot);
    return out;
}
