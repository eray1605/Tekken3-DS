// The game's pad word (PsyQ libetc layout, remake/platform/pad_state.gd) from the DS keys.
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

// The face buttons sit where the PlayStation's do: Y = Square, X = Triangle, B = Cross, A = Circle.
// L and R are L1 and R1; the DS has no L2/R2 (the touch screen will carry them).
uint16_t pad_from_keys(uint32_t keys);

// Writes the pressed buttons' names, space-separated ("-" when none), into out.
void pad_describe(uint16_t pad, char *out, int size);

#endif
