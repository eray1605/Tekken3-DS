// Pose vector (49 channels) → the rotations of the 18 local joint matrices
// (remake/core/fighter/pose_solver.gd).
//
// Rules from docs/research/formats/animation.md#pose-to-matrices: ten Euler joints, the limb basis,
// the normalised skeleton, and four two-bone IK limbs, with the game's GTE rounding.
#ifndef T3DS_CORE_FIGHTER_POSE_SOLVER_H
#define T3DS_CORE_FIGHTER_POSE_SOLVER_H

#include <stdbool.h>
#include <stdint.h>

#include "core/fighter/pose_tables.h"
#include "core/math/fx.h"

#define POSE_CHANNELS 49
#define POSE_SLOT_COUNT 18

typedef struct {
    const PoseTables *tables;
    bool wide_elbow;  // Gameplay fix #8: a 64-bit product for the arm bend
} PoseSolver;

// Writes the rotations of `slots` for one pose vector. Slots the IK leaves alone (targets closer
// than the minimum reach) keep their values, so slots persist between frames as in the game.
void pose_solver_build(const PoseSolver *solver, const int32_t pose[POSE_CHANNELS], Mat3 slots[POSE_SLOT_COUNT]);

// EulerToMatrix: Rz(z)·Ry(y)·Rx(x) with the game's rounding.
Mat3 pose_euler_to_matrix(const PoseTables *tables, int32_t x, int32_t y, int32_t z);

#endif
