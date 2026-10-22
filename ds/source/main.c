// Tekken 3 DS (docs/ds-port-plan.md): boots, times the ported pose solver, runs a 60 Hz loop over the
// game's pad word, draws a 3D test picture on the top screen and a status console on the bottom one.
#include <nds.h>
#include <stdio.h>

#include "dev/pose_bench.h"
#include "pad.h"
#include "test_scene.h"

#define LINES_PER_FRAME 263
#define VBLANK_LINE 192
#define BENCH_CALLS 600

// Scanlines spent since the last vertical blank began: the frame's CPU budget used so far.
static int lines_since_vblank(void)
{
    return (REG_VCOUNT - VBLANK_LINE + LINES_PER_FRAME) % LINES_PER_FRAME;
}

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
    printf(" build %s\n\n", T3DS_VERSION);
    printf(" Y X B A = Sqr Tri Crs Cir\n");
    printf(" Left/Right: turn the camera\n");

    PoseBenchResult bench = pose_bench_run(BENCH_CALLS);
    printf("\x1b[14;1Hpose solver, %lu calls:", (unsigned long)bench.calls);
    printf("\x1b[15;1H mean %lu us  best %lu  worst %lu", (unsigned long)bench.mean_us,
           (unsigned long)bench.best_us, (unsigned long)bench.worst_us);

    TestScene scene = {0, 0};
    uint32_t frame = 0;
    char buttons[64];

    while (1) {
        swiWaitForVBlank();
        scanKeys();
        uint16_t pad = pad_from_keys(keysHeld());

        test_scene_step(&scene, pad);
        test_scene_draw(&scene);
        glFlush(0);

        pad_describe(pad, buttons, sizeof(buttons));
        printf("\x1b[9;1Hframe %-8lu", (unsigned long)frame);
        printf("\x1b[10;1Hpad   %04X %-20s", pad, buttons);
        printf("\x1b[11;1Hspin  %4ld  orbit %4ld", (long)scene.spin, (long)scene.orbit);
        printf("\x1b[12;1Hcpu   %3d%%", lines_since_vblank() * 100 / LINES_PER_FRAME);
        frame++;
    }
}
