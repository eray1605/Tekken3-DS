#include "presentation/fighter_mesh.h"

static inline __attribute__((always_inline)) Vec3 rotate(const Mat3 *r, const ModelVertex *p)
{
    Vec3 out;
    for (int k = 0; k < 3; k++)
        out.v[k] = (r->m[3 * k] * p->x + r->m[3 * k + 1] * p->y + r->m[3 * k + 2] * p->z) >> 12;
    return out;
}

void fighter_mesh_pose_vertices(const ModelVertex *vertices, int count, const JointFrame *joints, const Vec3 *origin,
                                Vec3 *out)
{
    for (int i = 0; i < count; i++) {
        const JointFrame *joint = &joints[vertices[i].joint];
        Vec3 d = rotate(&joint->rot, &vertices[i]);
        for (int k = 0; k < 3; k++)
            out[i].v[k] = joint->t.v[k] + d.v[k] - origin->v[k];
    }
}

void fighter_mesh_light(const ModelVertex *normals, int count, const JointFrame *joints, const Vec3 *light,
                        int ambient, int diffuse, uint8_t *out)
{
    for (int i = 0; i < count; i++) {
        Vec3 w = rotate(&joints[normals[i].joint].rot, &normals[i]);
        int32_t cosine = (w.v[0] * light->v[0] + w.v[1] * light->v[1] + w.v[2] * light->v[2]) >> 12;
        int level = ambient + (cosine > 0 ? (cosine * diffuse) >> 12 : 0);
        out[i] = (uint8_t)(level > 31 ? 31 : level);
    }
}
