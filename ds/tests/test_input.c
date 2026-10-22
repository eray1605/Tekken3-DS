// The keys' path to the stance scene's commands: the DS keys → the game's pad word (pad.c), its
// edges from frame to frame, and the commands one frame's pad gives (stance_input.c).
#include "pad.h"
#include "stance_input.h"
#include "test.h"

void test_pad_maps_every_ds_key(void)
{
    static const struct {
        uint32_t key;
        uint16_t pad;
    } CASES[] = {
        {DS_KEY_Y, PAD_SQUARE}, {DS_KEY_X, PAD_TRIANGLE}, {DS_KEY_B, PAD_CROSS}, {DS_KEY_A, PAD_CIRCLE},
        {DS_KEY_L, PAD_L1}, {DS_KEY_R, PAD_R1}, {DS_KEY_SELECT, PAD_SELECT}, {DS_KEY_START, PAD_START},
        {DS_KEY_UP, PAD_UP}, {DS_KEY_RIGHT, PAD_RIGHT}, {DS_KEY_DOWN, PAD_DOWN}, {DS_KEY_LEFT, PAD_LEFT},
        {DS_KEY_TOUCH, 0}, {DS_KEY_LID, 0},
    };
    uint32_t all = 0;
    uint16_t want_all = 0;
    for (unsigned i = 0; i < sizeof(CASES) / sizeof(CASES[0]); i++) {
        EXPECT(pad_from_keys(CASES[i].key) == CASES[i].pad, "DS key 0x%X gives 0x%04X, expected 0x%04X",
               (unsigned)CASES[i].key, pad_from_keys(CASES[i].key), CASES[i].pad);
        all |= CASES[i].key;
        want_all |= CASES[i].pad;
    }
    EXPECT_EQ(pad_from_keys(all), want_all);
    EXPECT_EQ(pad_from_keys(0), 0);
    // Bits above the lid's are not keys.
    EXPECT_EQ(pad_from_keys(0xFFFFC000u), 0);
}

void test_pad_edges_once_per_press(void)
{
    PadState pad = {0, 0, 0};
    pad_update(&pad, DS_KEY_UP);
    EXPECT_EQ(pad.held, PAD_UP);
    EXPECT_EQ(pad.pressed, PAD_UP);
    EXPECT_EQ(pad.released, 0);
    for (int f = 0; f < 30; f++) {   // held for half a second: no further press
        pad_update(&pad, DS_KEY_UP);
        EXPECT_EQ(pad.pressed, 0);
    }
    pad_update(&pad, DS_KEY_UP | DS_KEY_A);
    EXPECT_EQ(pad.pressed, PAD_CIRCLE);
    pad_update(&pad, DS_KEY_A);
    EXPECT_EQ(pad.released, PAD_UP);
    EXPECT_EQ(pad.pressed, 0);
    pad_update(&pad, 0);
    EXPECT_EQ(pad.released, PAD_CIRCLE);
    pad_update(&pad, DS_KEY_UP);       // pressed again after a release
    EXPECT_EQ(pad.pressed, PAD_UP);
}

static StanceInput input_after(uint32_t before, uint32_t now)
{
    PadState pad = {0, 0, 0};
    pad_update(&pad, before);
    pad_update(&pad, now);
    return stance_input_from_pad(&pad);
}

void test_stance_commands_from_the_pad(void)
{
    StanceInput none = input_after(0, 0);
    EXPECT(!none.orbit && !none.key_step && !none.slot_step && !none.stance && !none.toggle_view, "no keys: nothing");

    EXPECT_EQ(input_after(0, DS_KEY_UP).key_step, 1);
    EXPECT_EQ(input_after(0, DS_KEY_DOWN).key_step, -1);
    EXPECT_EQ(input_after(DS_KEY_UP, DS_KEY_UP).key_step, 0);          // held: one change only
    EXPECT_EQ(input_after(0, DS_KEY_UP | DS_KEY_DOWN).key_step, 0);    // both at once cancel out
    EXPECT_EQ(input_after(DS_KEY_UP, DS_KEY_UP | DS_KEY_DOWN).key_step, -1);

    EXPECT_EQ(input_after(0, DS_KEY_A).slot_step, 1);
    EXPECT_EQ(input_after(0, DS_KEY_B).slot_step, -1);
    EXPECT_EQ(input_after(DS_KEY_A, DS_KEY_A).slot_step, 0);
    EXPECT_EQ(input_after(0, DS_KEY_A | DS_KEY_B).slot_step, 0);
    EXPECT_EQ(input_after(0, DS_KEY_X | DS_KEY_Y).slot_step, 0);       // △ and □ do nothing here

    // The camera turns while held, not only on the press.
    EXPECT_EQ(input_after(DS_KEY_LEFT, DS_KEY_LEFT).orbit, -1);
    EXPECT_EQ(input_after(0, DS_KEY_RIGHT).orbit, 1);
    EXPECT_EQ(input_after(0, DS_KEY_LEFT | DS_KEY_RIGHT).orbit, 0);

    EXPECT(input_after(0, DS_KEY_START).stance, "Start");
    EXPECT(!input_after(DS_KEY_START, DS_KEY_START).stance, "Start held");
    EXPECT(input_after(0, DS_KEY_SELECT).toggle_view, "Select");
    EXPECT(!input_after(DS_KEY_SELECT, DS_KEY_SELECT).toggle_view, "Select held");

    // Touch and lid give no command.
    StanceInput touch = input_after(0, DS_KEY_TOUCH | DS_KEY_LID);
    EXPECT(!touch.orbit && !touch.key_step && !touch.slot_step && !touch.stance && !touch.toggle_view,
           "touch and lid: nothing");
}
