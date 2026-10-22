#include "pad.h"

#include <nds.h>
#include <stdio.h>

static const struct {
    uint32_t key;
    uint16_t bit;
} KEY_MAP[] = {
    {KEY_Y, PAD_SQUARE}, {KEY_X, PAD_TRIANGLE}, {KEY_B, PAD_CROSS}, {KEY_A, PAD_CIRCLE},
    {KEY_L, PAD_L1}, {KEY_R, PAD_R1}, {KEY_SELECT, PAD_SELECT}, {KEY_START, PAD_START},
    {KEY_UP, PAD_UP}, {KEY_RIGHT, PAD_RIGHT}, {KEY_DOWN, PAD_DOWN}, {KEY_LEFT, PAD_LEFT},
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
