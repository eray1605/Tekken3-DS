// A fighter's joints for one frame (remake/core/fighter/fighter_skeleton.gd): pose vector → local
// rotations (PoseSolver) → root (RootUpdate) → world joint frames (FighterComposeJoints).
//
// Rules: docs/research/formats/animation.md#pose-to-matrices and docs/research/code/moves.md#root-motion.
// The costume attachments 18–21 follow their parent joint rigidly at the KMD offset.
// TODO: their swing (AttachmentDynamics, FUN_80037F10) when the fighter tables are converted.
#ifndef T3DS_CORE_FIGHTER_FIGHTER_SKELETON_H
#define T3DS_CORE_FIGHTER_FIGHTER_SKELETON_H

#include <stdint.h>

#include "core/fighter/joint_frame.h"
#include "core/fighter/pose_solver.h"
#include "core/fighter/pose_tables.h"

#define SKELETON_JOINTS 18      // joints built from the pose vector
#define SKELETON_ALL_JOINTS 22  // with the costume attachments (CharacterModel.PART_COUNT)

// A costume's joint layout, from its model (CharacterModel.joint_offsets, joint_parents, scale_fixed).
typedef struct {
    Vec3 offsets[SKELETON_ALL_JOINTS];   // local joint translation (the KMD's x, y, −z)
    int8_t parents[SKELETON_ALL_JOINTS]; // parent joint, −1 for the root's children
    int32_t scale;                       // model scale, 4.12
} FighterRig;

typedef struct {
    const PoseTables *tables;
    const FighterRig *rig;
    PoseSolver solver;
    Mat3 local_slots[SKELETON_JOINTS];  // persist between frames, as in the game
    JointFrame root;
    JointFrame joints[SKELETON_ALL_JOINTS];
} FighterSkeleton;

void fighter_skeleton_init(FighterSkeleton *skeleton, const PoseTables *tables, const FighterRig *rig);

// Root displacement of a pose vector (AnimDecodeRoot × model scale, AnimScaleRoot).
Vec3 fighter_skeleton_root_displacement(const FighterSkeleton *skeleton, const int32_t pose[POSE_CHANNELS]);

// Poses the skeleton on the ground at `anchor` (the move's anchor, fighter posX/Y/Z) with the given
// facing and heading (16-bit angles), no tilt.
void fighter_skeleton_update(FighterSkeleton *skeleton, const int32_t pose[POSE_CHANNELS], const Vec3 *anchor,
                             int32_t facing, int32_t heading);

// FUN_8003A6B4: Rx·Ry·Rz from three 16-bit angles with the game's GTE rounding.
Mat3 fighter_rotation_xyz(const PoseTables *tables, int32_t x, int32_t y, int32_t z);

#endif
