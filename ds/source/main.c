// Tekken 3 DS (docs/ds-port-plan.md): boots, times the ported pose solver, loads the converted data
// from NitroFS and shows one fighter in its stance (D1), posed by the C core at 60 Hz over the
// game's pad word, with a status console on the bottom screen: the pad, the fighter, the CPU time
// and the commands the keys gave. Without converted data it draws the bootstrap's 3D test picture.
#include <nds.h>
#include <stdio.h>

#include "dev/input_script.h"
#include "dev/pose_bench.h"
#include "pad.h"
#include "platform/files.h"
#include "stance_scene.h"
#include "test_scene.h"

#define LINES_PER_FRAME 263
#define VBLANK_LINE 192
#define BENCH_CALLS 600
#define FIRST_KEY 0   // Paul's first costume

// pad.c reads keysHeld() with its own names for libnds's bits.
#define SAME_KEY(name) ((int)DS_KEY_##name == (int)KEY_##name)
_Static_assert(SAME_KEY(A) && SAME_KEY(B) && SAME_KEY(SELECT) && SAME_KEY(START) && SAME_KEY(RIGHT) && SAME_KEY(LEFT) &&
                   SAME_KEY(UP) && SAME_KEY(DOWN) && SAME_KEY(R) && SAME_KEY(L) && SAME_KEY(X) && SAME_KEY(Y) &&
                   SAME_KEY(TOUCH) && SAME_KEY(LID),
               "pad.h's DS key bits differ from libnds's");

// Scanlines spent since the last vertical blank began: the frame's CPU budget used so far.
static int lines_since_vblank(void)
{
    return (REG_VCOUNT - VBLANK_LINE + LINES_PER_FRAME) % LINES_PER_FRAME;
}

static StanceScene stance;

static void print_status(uint32_t frame, const PadState *pad)
{
    char buttons[64];
    pad_describe(pad->held, buttons, sizeof(buttons));
    printf("\x1b[8;0Hframe %-7lu pad %04X %-12.12s", (unsigned long)frame, pad->held, buttons);
    const StanceFighter *f = &stance.fighters[stance.shown];
    const CharacterEntry *c = stance_scene_character(&stance);
    printf("\x1b[9;0Hkey %2d %-10.10s cos %2d mot%02d", f->key, c->name, c->costume_slot, c->bank_type);
    printf("\x1b[10;0Hslot %4d frame %3d/%-3d", stance.slot, stance.frame, stance.stream.frames);
    printf("\x1b[11;0Hcore %4lu us (sample %3lu) w%4lu", (unsigned long)stance.core_us,
           (unsigned long)stance.sample_us, (unsigned long)stance.core_worst_us);
    printf("\x1b[12;0Hdraw %4lu us (pose %4lu)      ", (unsigned long)stance.draw_us,
           (unsigned long)timerTicks2usec(f->view.pose_ticks));
    printf("\x1b[13;0H%3lu polys %3lu KB textures     ", (unsigned long)f->model.polygon_count,
           (unsigned long)(f->view.texture_bytes / 1024));
    printf("\x1b[15;0Hcommands:");
    for (int i = 0; i < STANCE_LOG_LINES; i++)
        printf("\x1b[%d;0H %-31s", 16 + i, stance.log[i]);
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    videoSetMode(MODE_0_3D);
    vramSetBankA(VRAM_A_TEXTURE);
    vramSetBankB(VRAM_B_TEXTURE);
    vramSetBankD(VRAM_D_TEXTURE);   // a key change holds the old and the new textures for a moment
    vramSetBankE(VRAM_E_TEX_PALETTE);
    consoleDemoInit();
    lcdMainOnTop();
    test_scene_init_gl();

    printf("\x1b[0;0HTekken 3 DS  D1: one fighter\n");
    printf("build %s\n", T3DS_VERSION);

    bool fighter = files_init() && stance_scene_init(&stance, FIRST_KEY);
    if (fighter) {
        printf("\x1b[3;0HUp/Down costume   Left/Right cam");
        printf("\x1b[4;0HA/B move slot     Start stance");
        printf("\x1b[5;0HSelect skeleton/model");
    } else {
        printf("\x1b[3;0Hno converted data: %s", stance.error ? stance.error : "NitroFS");
        printf("\x1b[4;0H(make convert-ds, then make nds)");
    }

    PoseBenchResult bench = pose_bench_run(BENCH_CALLS);
    printf("\x1b[21;0Hpose solver (%lu calls):", (unsigned long)bench.calls);
    printf("\x1b[22;0H mean %lu best %lu worst %lu us", (unsigned long)bench.mean_us, (unsigned long)bench.best_us,
           (unsigned long)bench.worst_us);

    TestScene scene = {0, 0};
    PadState pad = {0, 0, 0};
    uint32_t frame = 0;

    while (1) {
        swiWaitForVBlank();
        scanKeys();
#ifdef T3DS_INPUT_SCRIPT
        pad_update(&pad, input_script_keys(frame));
#else
        pad_update(&pad, keysHeld());
#endif

        if (fighter) {
            stance_scene_step(&stance, &pad);
            stance_scene_draw(&stance);
        } else {
            test_scene_step(&scene, pad.held);
            test_scene_draw(&scene);
        }
        glFlush(0);

        if (fighter)
            print_status(frame, &pad);
        printf("\x1b[23;0Hcpu %3d%%", lines_since_vblank() * 100 / LINES_PER_FRAME);
        frame++;
    }
}
