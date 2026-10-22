#include "pad.h"

#include <stdio.h>

static const struct {
    uint32_t key;
    uint16_t bit;
} KEY_MAP[] = {
    {DS_KEY_Y, PAD_SQUARE}, {DS_KEY_X, PAD_TRIANGLE}, {DS_KEY_B, PAD_CROSS}, {DS_KEY_A, PAD_CIRCLE},
    {DS_KEY_L, PAD_L1}, {DS_KEY_R, PAD_R1}, {DS_KEY_SELECT, PAD_SELECT}, {DS_KEY_START, PAD_START},
    {DS_KEY_UP, PAD_UP}, {DS_KEY_RIGHT, PAD_RIGHT}, {DS_KEY_DOWN, PAD_DOWN}, {DS_KEY_LEFT, PAD_LEFT},
};

static const struct {
    uint16_t bit;
    const char *name;
} NAMES[] = {
    {PAD_L2, "L2"}, {PAD_R2, "R2"}, {PAD_L1, "L1"}, {PAD_R1, "R1"},
    {PAD_TRIANGLE, "Tri"}, {PAD_CIRCLE, "Cir"}, {PAD_CROSS, "Crs"}, {PAD_SQUARE, "Sqr"},
    {PAD_SELECT, "Sel"}, {PAD_START, "Start"},
    {PAD_UP, "Up"}, {PAD_RIGHT, "Right"}, {PAD_DOWN, "Down"}, {PAD_LEFT, "Left"},
};

uint16_t pad_from_keys(uint32_t keys)
{
    uint16_t pad = 0;
    for (unsigned i = 0; i < sizeof(KEY_MAP) / sizeof(KEY_MAP[0]); i++)
        if (keys & KEY_MAP[i].key)
            pad |= KEY_MAP[i].bit;
    return pad;
}

void pad_update(PadState *state, uint32_t keys)
{
    uint16_t held = pad_from_keys(keys);
    state->pressed = held & (uint16_t)~state->held;
    state->released = state->held & (uint16_t)~held;
    state->held = held;
}

void pad_describe(uint16_t pad, char *out, int size)
{
    int used = 0;
    out[0] = '\0';
    for (unsigned i = 0; i < sizeof(NAMES) / sizeof(NAMES[0]) && used < size; i++)
        if (pad & NAMES[i].bit)
            used += snprintf(out + used, size - used, used ? " %s" : "%s", NAMES[i].name);
    if (used == 0)
        snprintf(out, size, "-");
}
