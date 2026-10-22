#include "stance_input.h"

static int axis(uint16_t bits, uint16_t minus, uint16_t plus)
{
    return ((bits & plus) ? 1 : 0) - ((bits & minus) ? 1 : 0);
}

StanceInput stance_input_from_pad(const PadState *pad)
{
    StanceInput in;
    in.orbit = axis(pad->held, PAD_LEFT, PAD_RIGHT);
    in.key_step = axis(pad->pressed, PAD_DOWN, PAD_UP);
    in.slot_step = axis(pad->pressed, PAD_CROSS, PAD_CIRCLE);
    in.stance = (pad->pressed & PAD_START) != 0;
    in.toggle_view = (pad->pressed & PAD_SELECT) != 0;
    return in;
}
