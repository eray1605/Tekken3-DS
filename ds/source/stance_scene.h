// D1's fighter on the DS: one costume key's rig, model and motion bank loaded from NitroFS, a move
// slot's animation (the stance, slot 3, by default) sampled each frame and posed by the C core,
// drawn as the textured model (or the wire skeleton) over a floor, with the CPU time per frame.
//
// A costume key is loaded whole into the spare fighter slot and shown only when all of it loaded:
// a key that fails keeps the fighter on screen as it was, with the error reported.
#ifndef T3DS_STANCE_SCENE_H
#define T3DS_STANCE_SCENE_H

#include <stdbool.h>
#include <stdint.h>

#include "content/characters_file.h"
#include "content/model_file.h"
#include "core/fighter/anim_stream.h"
#include "core/fighter/fighter_skeleton.h"
#include "core/fighter/motion_bank.h"
#include "pad.h"
#include "presentation/fighter_view.h"

#define STANCE_SLOT 3
#define STANCE_LOG_LINES 4
#define STANCE_LOG_CHARS 32

// Everything one costume key needs.
typedef struct {
    int key;
    uint8_t *bank_data;
    MotionBank bank;
    FighterRig rig;
    uint8_t *model_data;
    FighterModel model;
    FighterView view;
} StanceFighter;

typedef struct {
    // Loaded once.
    PoseTables tables;
    uint8_t *spline_file;
    SplineTables spline;
    CharacterTable characters;
    uint8_t *common_data;
    MotionBank common;
    // The shown costume key and the spare slot the next one loads into.
    StanceFighter fighters[2];
    int shown;
    MotionSet set;
    FighterSkeleton skeleton;
    bool show_skeleton;          // the wire skeleton instead of the model
    // The animation.
    int slot;
    AnimStream stream;
    int frame;
    int32_t orbit;               // camera angle around the fighter, game units
    uint32_t core_us;            // sampling and skeleton of the last step
    uint32_t sample_us;          // of which sampling the pose
    uint32_t core_worst_us;
    uint32_t draw_us;            // posing, lighting and sending the model
    uint32_t steps;              // frames stepped
    // The commands the pad gave, newest last.
    char log[STANCE_LOG_LINES][STANCE_LOG_CHARS];
    const char *error;           // what failed to load last, NULL when all is well
} StanceScene;

// Loads the tables and the common bank, then `key`; false (and `error`) when data is missing.
bool stance_scene_init(StanceScene *scene, int key);

// The shown costume key's entry.
const CharacterEntry *stance_scene_character(const StanceScene *scene);

// Another costume key (its bank, rig and model); the slot goes back to the stance. On failure the
// shown fighter stays and `error` names the file.
bool stance_scene_load_key(StanceScene *scene, int key);

// The pad's commands (stance_input.h), then one animation frame.
void stance_scene_step(StanceScene *scene, const PadState *pad);

void stance_scene_draw(StanceScene *scene);

#endif
