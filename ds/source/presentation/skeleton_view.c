#include "presentation/skeleton_view.h"

#include <nds.h>

#define CROSS 24  // half the joint marker's size, game units

// The 3D engine draws a triangle with two equal corners as a line.
static void line(const Vec3 *a, const Vec3 *b)
{
    glVertex3v16((v16)a->v[0], (v16)a->v[1], (v16)a->v[2]);
    glVertex3v16((v16)b->v[0], (v16)b->v[1], (v16)b->v[2]);
    glVertex3v16((v16)b->v[0], (v16)b->v[1], (v16)b->v[2]);
}

static void cross(const Vec3 *p)
{
    for (int k = 0; k < 3; k++) {
        Vec3 a = *p;
        Vec3 b = *p;
        a.v[k] -= CROSS;
        b.v[k] += CROSS;
        line(&a, &b);
    }
}

void skeleton_view_draw(const FighterSkeleton *skeleton)
{
    const FighterRig *rig = skeleton->rig;
    glBindTexture(0, 0);
    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE);
    glBegin(GL_TRIANGLES);
    for (int m = 0; m < SKELETON_ALL_JOINTS; m++) {
        const JointFrame *parent = rig->parents[m] < 0 ? &skeleton->root : &skeleton->joints[rig->parents[m]];
        if (m < SKELETON_JOINTS)
            glColor3b(250, 210, 60);
        else
            glColor3b(90, 220, 120);   // costume attachments
        line(&parent->t, &skeleton->joints[m].t);
    }
    glColor3b(240, 80, 60);
    for (int m = 0; m < SKELETON_ALL_JOINTS; m++)
        cross(&skeleton->joints[m].t);
    glColor3b(80, 160, 255);
    cross(&skeleton->root.t);
    glEnd();
}
