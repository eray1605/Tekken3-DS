// What the pad asks of the stance scene in one frame (stance_scene.c), kept apart so that the keys'
// commands are tested on the host. Opposite buttons together cancel out.
#ifndef T3DS_STANCE_INPUT_H
#define T3DS_STANCE_INPUT_H

#include <stdbool.h>

#include "pad.h"

typedef struct {
    int orbit;          // camera turn while held: Left −1, Right +1
    int key_step;       // costume key on press: Up +1, Down −1
    int slot_step;      // move slot on press: ○ (A) +1, ✕ (B) −1
    bool stance;        // Start: back to the stance (after a slot step of the same frame)
    bool toggle_view;   // Select: the wire skeleton or the model
} StanceInput;

StanceInput stance_input_from_pad(const PadState *pad);

#endif
