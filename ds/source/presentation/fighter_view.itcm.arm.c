#include "presentation/fighter_view.h"

#include <nds.h>

#include "presentation/fighter_mesh.h"

#define AMBIENT 13
#define DIFFUSE 20
// Towards the light: from the front, above and the left (game axes, y down), 4.12 unit vector.
static const Vec3 LIGHT = {{-1365, -2731, -2731}};

bool fighter_view_init(FighterView *view, const FighterModel *model)
{
    view->model = model;
    view->texture_bytes = 0;
    if (model->vertex_count > FIGHTER_VIEW_VERTICES_MAX || model->normal_count > FIGHTER_VIEW_NORMALS_MAX ||
        model->corner_count > FIGHTER_VIEW_CORNERS_MAX) {
        view->model = NULL;
        return false;
    }
    for (int i = 0; i < model->vertex_count; i++)
        view->model_vertices[i] = model_vertex(model, i);
    for (int i = 0; i < model->normal_count; i++)
        view->model_normals[i] = model_normal(model, i);
    for (uint32_t c = 0; c < model->corner_count; c++) {
        ModelCorner corner = model_corner(model, c);
        view->corners[c] = (FighterViewCorner){corner.a, corner.b, corner.normal, corner.weight,
                                               TEXTURE_PACK(corner.u << 4, corner.v << 4)};
    }
    for (int m = 0; m < model->material_count; m++)
        view->textures[m] = 0;
    glGenTextures(model->material_count, view->textures);
    for (int m = 0; m < model->material_count; m++) {
        const ModelMaterial *mat = &model->materials[m];
        int params = TEXGEN_TEXCOORD | (mat->flags & MODEL_TRANSPARENT_INDEX0 ? GL_TEXTURE_COLOR0_TRANSPARENT : 0);
        glBindTexture(0, view->textures[m]);
        if (!glTexImage2D(0, 0, mat->depth == 4 ? GL_RGB16 : GL_RGB256, mat->width, mat->height, 0, params,
                          mat->texels) ||
            !glColorTableEXT(0, 0, mat->palette_size, 0, 0, mat->palette)) {
            fighter_view_release(view);
            return false;
        }
        view->texture_bytes += (uint32_t)mat->width * mat->height * mat->depth / 8 + 2u * mat->palette_size;
    }
    return true;
}

void fighter_view_release(FighterView *view)
{
    if (view->model)
        glDeleteTextures(view->model->material_count, view->textures);
    view->model = NULL;
}

void fighter_view_draw(FighterView *view, const FighterSkeleton *skeleton)
{
    const FighterModel *model = view->model;
    const Vec3 *origin = &skeleton->root.t;
    uint32_t start = cpuGetTiming();
    fighter_mesh_pose_vertices(view->model_vertices, model->vertex_count, skeleton->joints, origin, view->vertices);
    fighter_mesh_light(view->model_normals, model->normal_count, skeleton->joints, &LIGHT, AMBIENT, DIFFUSE,
                       view->light);
    for (int n = 0; n < model->normal_count; n++)
        view->colours[n] = RGB15(view->light[n], view->light[n], view->light[n]);
    view->pose_ticks = cpuGetTiming() - start;

    glPushMatrix();
    glTranslatef32(origin->v[0], origin->v[1], origin->v[2]);
    for (int b = 0; b < model->batch_count; b++) {
        ModelBatch batch = model_batch(model, b);
        glBindTexture(0, view->textures[batch.material]);
        // The scale's y flip turns the screen winding over: the game's front faces cull as back here.
        glPolyFmt(POLY_ALPHA(31) | (batch.flags & MODEL_DOUBLE_SIDED ? POLY_CULL_NONE : POLY_CULL_FRONT));
        GFX_BEGIN = batch.corners == 4 ? GL_QUADS : GL_TRIANGLES;
        const FighterViewCorner *corner = &view->corners[batch.first];
        const FighterViewCorner *end = corner + batch.polygons * batch.corners;
        for (; corner < end; corner++) {
            Vec3 p = fighter_mesh_corner(&view->vertices[corner->a], &view->vertices[corner->b], corner->weight);
            GFX_COLOR = view->colours[corner->normal];
            GFX_TEX_COORD = corner->texcoord;
            GFX_VERTEX16 = (uint32_t)(uint16_t)p.v[0] | (uint32_t)(uint16_t)p.v[1] << 16;
            GFX_VERTEX16 = (uint32_t)(uint16_t)p.v[2];
        }
        GFX_END = 0;
    }
    glPopMatrix(1);
}
