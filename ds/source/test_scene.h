// The bootstrap's 3D test picture: a checkered floor and a turning cube. The state is kept in the
// game's units (4096 angle units per turn, remake/core/math) and only converted when drawn, the
// split the ported core and the DS presentation will keep.
#ifndef T3DS_TEST_SCENE_H
#define T3DS_TEST_SCENE_H

#include <stdint.h>

typedef struct {
    int32_t spin;   // the cube's turn
    int32_t orbit;  // the camera's angle around the cube
} TestScene;

void test_scene_init_gl(void);
void test_scene_step(TestScene *scene, uint16_t pad);
void test_scene_draw(const TestScene *scene);

#endif
