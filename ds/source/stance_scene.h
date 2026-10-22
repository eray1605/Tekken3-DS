// D1's fighter on the DS: one costume key's rig and motion bank loaded from NitroFS, a move slot's
// animation (the stance, slot 3, by default) sampled each frame and posed by the C core, drawn as a
// wire skeleton over a floor, with the core's CPU time per frame.
#ifndef T3DS_STANCE_SCENE_H
#define T3DS_STANCE_SCENE_H

#include <stdbool.h>
#include <stdint.h>

#include "content/characters_file.h"
#include "core/fighter/anim_stream.h"
#include "core/fighter/fighter_skeleton.h"
#include "core/fighter/motion_bank.h"

#define STANCE_SLOT 3

typedef struct {
    // Loaded once.
    PoseTables tables;
    uint8_t *spline_file;
    SplineTables spline;
    CharacterTable characters;
    uint8_t *common_data;
    MotionBank common;
    // Per costume key.
    int key;
    int bank_type;               // the loaded own bank, −1: none
    uint8_t *own_data;
    MotionBank own;
    MotionSet set;
    FighterRig rig;
    FighterSkeleton skeleton;
    // The animation.
    int slot;
    AnimStream stream;
    int frame;
    int32_t orbit;               // camera angle around the fighter, game units
    uint32_t core_us;            // sampling and skeleton of the last step
    uint32_t sample_us;          // of which sampling the pose
    uint32_t core_worst_us;
    const char *error;           // what failed to load, NULL when all is well
} StanceScene;

// Loads the tables and the common bank, then `key`; false (and `error`) when data is missing.
bool stance_scene_init(StanceScene *scene, int key);

// Another costume key (its bank and rig); the slot goes back to the stance.
bool stance_scene_load_key(StanceScene *scene, int key);

// Left/Right turn the camera, Up/Down change the costume key, ○/✕ the move slot, Start resets to
// the stance.
void stance_scene_step(StanceScene *scene, uint16_t held, uint16_t pressed);

void stance_scene_draw(const StanceScene *scene);

#endif
