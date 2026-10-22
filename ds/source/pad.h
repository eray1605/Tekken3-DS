// The game's pad word (PsyQ libetc layout, remake/platform/pad_state.gd) from the DS keys, and its
// changes from frame to frame. Plain C: the DS key bits are libnds's keysHeld() layout (the KEYINPUT
// register's bits 0–9, then X, Y, touch and lid), so the mapping is tested on the host.
#ifndef T3DS_PAD_H
#define T3DS_PAD_H

#include <stdint.h>

enum {
    PAD_L2 = 0x0001,
    PAD_R2 = 0x0002,
    PAD_L1 = 0x0004,
    PAD_R1 = 0x0008,
    PAD_TRIANGLE = 0x0010,
    PAD_CIRCLE = 0x0020,
    PAD_CROSS = 0x0040,
    PAD_SQUARE = 0x0080,
    PAD_SELECT = 0x0100,
    PAD_START = 0x0800,
    PAD_UP = 0x1000,
    PAD_RIGHT = 0x2000,
    PAD_DOWN = 0x4000,
    PAD_LEFT = 0x8000,
};

// keysHeld()'s bits (libnds KEY_*).
enum {
    DS_KEY_A = 1 << 0,
    DS_KEY_B = 1 << 1,
    DS_KEY_SELECT = 1 << 2,
    DS_KEY_START = 1 << 3,
    DS_KEY_RIGHT = 1 << 4,
    DS_KEY_LEFT = 1 << 5,
    DS_KEY_UP = 1 << 6,
    DS_KEY_DOWN = 1 << 7,
    DS_KEY_R = 1 << 8,
    DS_KEY_L = 1 << 9,
    DS_KEY_X = 1 << 10,
    DS_KEY_Y = 1 << 11,
    DS_KEY_TOUCH = 1 << 12,
    DS_KEY_LID = 1 << 13,
};

// The pad word this frame and its changes since the last.
typedef struct {
    uint16_t held;
    uint16_t pressed;   // held now, not in the last frame
    uint16_t released;  // held in the last frame, not now
} PadState;

// The face buttons sit where the PlayStation's do: Y = Square, X = Triangle, B = Cross, A = Circle.
// L and R are L1 and R1; the DS has no L2/R2 (the touch screen will carry them). Touch and lid are
// not buttons of the pad.
uint16_t pad_from_keys(uint32_t keys);

// The next frame's state from the DS keys held now.
void pad_update(PadState *state, uint32_t keys);

// Writes the pressed buttons' names, space-separated ("-" when none), into out.
void pad_describe(uint16_t pad, char *out, int size);

#endif
