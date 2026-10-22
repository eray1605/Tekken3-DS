// Tekken 3 DS (docs/ds-port-plan.md): boots, times the ported pose solver, loads the converted data
// from NitroFS and shows one fighter in its stance as a wire skeleton (D1), posed by the C core at
// 60 Hz over the game's pad word, with a status console on the bottom screen. Without converted
// data it draws the bootstrap's 3D test picture.
#include <nds.h>
#include <stdio.h>

#include "dev/pose_bench.h"
#include "pad.h"
#include "platform/files.h"
#include "stance_scene.h"
#include "test_scene.h"

#define LINES_PER_FRAME 263
#define VBLANK_LINE 192
#define BENCH_CALLS 600
#define FIRST_KEY 0   // Paul's first costume

// Scanlines spent since the last vertical blank began: the frame's CPU budget used so far.
static int lines_since_vblank(void)
{
    return (REG_VCOUNT - VBLANK_LINE + LINES_PER_FRAME) % LINES_PER_FRAME;
}

static StanceScene stance;

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    videoSetMode(MODE_0_3D);
    vramSetBankA(VRAM_A_TEXTURE);
    consoleDemoInit();
    lcdMainOnTop();
    test_scene_init_gl();

    printf("\x1b[1;1HTekken 3 DS\n");
    printf(" milestone D1: one fighter\n");
    printf(" build %s\n", T3DS_VERSION);

    bool fighter = files_init() && stance_scene_init(&stance, FIRST_KEY);
    if (fighter) {
        printf("\x1b[5;1HUp/Down costume  L/R camera");
        printf("\x1b[6;1HA/B move slot    Start stance");
    } else {
        printf("\x1b[5;1Hno converted data: %s", stance.error ? stance.error : "NitroFS");
        printf("\x1b[6;1H(make convert-ds, then make nds)");
    }

    PoseBenchResult bench = pose_bench_run(BENCH_CALLS);
    printf("\x1b[20;1Hpose solver, %lu calls:", (unsigned long)bench.calls);
    printf("\x1b[21;1H mean %lu best %lu worst %lu us", (unsigned long)bench.mean_us,
           (unsigned long)bench.best_us, (unsigned long)bench.worst_us);

    TestScene scene = {0, 0};
    uint32_t frame = 0;
    uint16_t last_pad = 0;
    char buttons[64];

    while (1) {
        swiWaitForVBlank();
        scanKeys();
        uint16_t pad = pad_from_keys(keysHeld());
        uint16_t pressed = pad & ~last_pad;
        last_pad = pad;

        if (fighter) {
            stance_scene_step(&stance, pad, pressed);
            stance_scene_draw(&stance);
        } else {
            test_scene_step(&scene, pad);
            test_scene_draw(&scene);
        }
        glFlush(0);

        pad_describe(pad, buttons, sizeof(buttons));
        printf("\x1b[9;1Hframe %-8lu", (unsigned long)frame);
        printf("\x1b[10;1Hpad   %04X %-20s", pad, buttons);
        if (fighter) {
            const CharacterEntry *c = &stance.characters.keys[stance.key];
            printf("\x1b[12;1Hkey   %2d %-13s", stance.key, c->name);
            printf("\x1b[13;1Hcostume %2d  bank divmot%02d", c->costume_slot, c->bank_type);
            printf("\x1b[14;1Hslot  %4d  frame %3d/%-3d", stance.slot, stance.frame, stance.stream.frames);
            printf("\x1b[15;1Hcore  %4lu us  worst %4lu us", (unsigned long)stance.core_us,
                   (unsigned long)stance.core_worst_us);
            printf("\x1b[16;1H sample %4lu  skeleton %4lu us", (unsigned long)stance.sample_us,
                   (unsigned long)(stance.core_us - stance.sample_us));
        }
        printf("\x1b[18;1Hcpu   %3d%%", lines_since_vblank() * 100 / LINES_PER_FRAME);
        frame++;
    }
}
