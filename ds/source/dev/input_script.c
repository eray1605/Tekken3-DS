#include "dev/input_script.h"

#include "pad.h"

// Each step holds its keys for `frames` frames, then nothing for as long. From Paul's first
// costume (key 0) it ends on King's first costume (key 12) in the stance (slot 3) with the model
// shown, the last commands "Start stance, slot 3", "Dn key 16 YOSHIMITSU", "Dn key 13 KING",
// "Dn key 12 KING".
static const struct {
    uint32_t keys;
    uint16_t frames;
    uint16_t repeat;
} STEPS[] = {
    {DS_KEY_UP, 6, 10},                  // ten costume keys up, quickly: 0 → 17
    {DS_KEY_A, 6, 2},                    // two move slots on
    {DS_KEY_B, 6, 1},                    // one back
    {DS_KEY_SELECT, 6, 2},               // skeleton, then the model again
    {DS_KEY_LEFT, 40, 1},                // the camera turns while held
    {DS_KEY_START, 6, 1},                // back to the stance
    {DS_KEY_UP | DS_KEY_DOWN, 6, 1},     // both together: nothing
    {DS_KEY_TOUCH | DS_KEY_LID, 6, 1},   // not pad buttons: nothing
    {DS_KEY_DOWN, 6, 3},                 // three keys down: 17 → 12
};
#define START_FRAME 60   // after the first load

uint32_t input_script_keys(uint32_t frame)
{
    if (frame < START_FRAME)
        return 0;
    uint32_t t = frame - START_FRAME;
    for (unsigned i = 0; i < sizeof(STEPS) / sizeof(STEPS[0]); i++) {
        uint32_t period = 2u * STEPS[i].frames;
        uint32_t length = period * STEPS[i].repeat;
        if (t < length)
            return t % period < STEPS[i].frames ? STEPS[i].keys : 0;
        t -= length;
    }
    return 0;
}
