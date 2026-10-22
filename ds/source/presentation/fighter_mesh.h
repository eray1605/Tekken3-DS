// A costume's model (its vertices and normals unpacked, model_vertex) posed by a skeleton's joint frames on the CPU, for drawing: every vertex in
// its joint's frame, then each corner as its vertex or, on a seam, the 0.5/0.5 blend of two (the
// exchange of DrawSkinnedPart). Positions are game units relative to an origin (the root), so that
// they fit the 3D engine's 16-bit vertices. Plain C, no libnds: tested on the host.
#ifndef T3DS_PRESENTATION_FIGHTER_MESH_H
#define T3DS_PRESENTATION_FIGHTER_MESH_H

#include <stdint.h>

#include "content/model_file.h"
#include "core/fighter/joint_frame.h"

// Each of `count` vertices: t + (R · p) >> 12 of its joint, minus `origin`.
void fighter_mesh_pose_vertices(const ModelVertex *vertices, int count, const JointFrame *joints, const Vec3 *origin,
                                Vec3 *out);

// A corner's position from its posed vertices: a + ((b − a) · weight) >> 8 (weight in 1/256).
static inline Vec3 fighter_mesh_corner(const Vec3 *a, const Vec3 *b, int weight)
{
    Vec3 out = *a;
    if (weight)
        for (int k = 0; k < 3; k++)
            out.v[k] += ((b->v[k] - a->v[k]) * weight) >> 8;
    return out;
}

// Each normal's light level 0…31: `ambient` plus `diffuse` times the cosine between the normal in
// its joint's frame and `light` (a 4.12 unit vector towards the light), when the normal faces it.
void fighter_mesh_light(const ModelVertex *normals, int count, const JointFrame *joints, const Vec3 *light,
                        int ambient, int diffuse, uint8_t *out);

#endif
