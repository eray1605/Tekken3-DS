#include "test_scene.h"

#include <nds.h>

#include "pad.h"

#define GAME_TURN 4096
#define SPIN_PER_FRAME (GAME_TURN / 240)  // one turn in four seconds
#define ORBIT_PER_FRAME 32
#define FLOOR_TILES 8                     // per side, one unit each
#define CAMERA_DISTANCE 3
#define CAMERA_HEIGHT floattof32(1.25)

// libnds angles run 32768 per turn.
static inline int ds_angle(int32_t game_angle)
{
    return game_angle << 3;
}

void test_scene_init_gl(void)
{
    glInit();
    glEnable(GL_ANTIALIAS);
    glEnable(GL_TEXTURE_2D);
    glClearColor(2, 4, 10, 31);
    glClearPolyID(63);
    glClearDepth(GL_MAX_DEPTH);
    glViewport(0, 0, 255, 191);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(60, 256.0 / 192.0, 0.1, 40);
}

void test_scene_step(TestScene *scene, uint16_t pad)
{
    scene->spin = (scene->spin + SPIN_PER_FRAME) & (GAME_TURN - 1);
    if (pad & PAD_LEFT)
        scene->orbit = (scene->orbit - ORBIT_PER_FRAME) & (GAME_TURN - 1);
    if (pad & PAD_RIGHT)
        scene->orbit = (scene->orbit + ORBIT_PER_FRAME) & (GAME_TURN - 1);
}

static void draw_floor(void)
{
    glBindTexture(0, 0);
    glBegin(GL_QUADS);
    for (int z = -FLOOR_TILES / 2; z < FLOOR_TILES / 2; z++) {
        for (int x = -FLOOR_TILES / 2; x < FLOOR_TILES / 2; x++) {
            if ((x + z) & 1)
                glColor3b(70, 70, 90);
            else
                glColor3b(150, 150, 170);
            glVertex3v16(inttov16(x), 0, inttov16(z));
            glVertex3v16(inttov16(x), 0, inttov16(z + 1));
            glVertex3v16(inttov16(x + 1), 0, inttov16(z + 1));
            glVertex3v16(inttov16(x + 1), 0, inttov16(z));
        }
    }
    glEnd();
}

static void draw_cube(void)
{
    glBindTexture(0, 0);
    static const v16 H = floattov16(0.5);
    static const v16 CORNERS[8][3] = {
        {-H, -H, -H}, {H, -H, -H}, {H, H, -H}, {-H, H, -H},
        {-H, -H, H}, {H, -H, H}, {H, H, H}, {-H, H, H},
    };
    static const uint8_t FACES[6][4] = {
        {0, 1, 2, 3}, {5, 4, 7, 6}, {4, 0, 3, 7}, {1, 5, 6, 2}, {3, 2, 6, 7}, {4, 5, 1, 0},
    };
    static const uint8_t COLORS[6][3] = {
        {230, 60, 40}, {40, 90, 230}, {240, 200, 40}, {60, 200, 80}, {240, 240, 240}, {120, 60, 160},
    };

    glBegin(GL_QUADS);
    for (int f = 0; f < 6; f++) {
        glColor3b(COLORS[f][0], COLORS[f][1], COLORS[f][2]);
        for (int v = 0; v < 4; v++) {
            const v16 *c = CORNERS[FACES[f][v]];
            glVertex3v16(c[0], c[1], c[2]);
        }
    }
    glEnd();
}

void test_scene_draw(const TestScene *scene)
{
    int orbit = ds_angle(scene->orbit);
    int32_t eye_x = sinLerp(orbit) * CAMERA_DISTANCE;
    int32_t eye_z = cosLerp(orbit) * CAMERA_DISTANCE;

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAtf32(eye_x, CAMERA_HEIGHT, eye_z, 0, floattof32(0.5), 0, 0, inttof32(1), 0);
    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE);

    draw_floor();

    glPushMatrix();
    glTranslatef32(0, floattof32(0.75), 0);
    glRotateYi(ds_angle(scene->spin));
    glRotateXi(ds_angle(scene->spin / 2));
    draw_cube();
    glPopMatrix(1);
}
