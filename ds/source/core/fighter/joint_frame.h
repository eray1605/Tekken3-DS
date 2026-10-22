// A joint's rotation (4.12, row-major) and translation in the game's world units
// (remake/core/fighter/joint_frame.gd).
#ifndef T3DS_CORE_FIGHTER_JOINT_FRAME_H
#define T3DS_CORE_FIGHTER_JOINT_FRAME_H

#include "core/math/fx.h"

typedef struct {
    Mat3 rot;
    Vec3 t;
} JointFrame;

// ComposeJoint / ComposeJointOffset: R = P.R·L (MVMVA per column, s16), and t = P.t + P.R·offset,
// the product read from the 32-bit MAC registers (not saturated). The offset is loaded into the
// 16-bit GTE vector registers, so it is truncated to s16.
JointFrame joint_frame_compose_offset(const JointFrame *parent, const Mat3 *local_rot, const Vec3 *offset);

#endif
