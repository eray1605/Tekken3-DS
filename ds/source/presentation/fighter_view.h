// A costume's model on the 3D engine: its materials as paletted textures (4 or 8 bits per texel,
// as on the PlayStation) and its polygons, posed on the CPU each frame (fighter_mesh) and lit by
// one directional light. The modelview must hold the world transform for game units; the view
// translates to the skeleton's root itself.
#ifndef T3DS_PRESENTATION_FIGHTER_VIEW_H
#define T3DS_PRESENTATION_FIGHTER_VIEW_H

#include <stdbool.h>

#include "content/model_file.h"
#include "core/fighter/fighter_skeleton.h"

#define FIGHTER_VIEW_VERTICES_MAX 1024
#define FIGHTER_VIEW_NORMALS_MAX 1024
#define FIGHTER_VIEW_CORNERS_MAX 3328

// A corner unpacked once, as the draw loop sends it.
typedef struct {
    uint16_t a, b, normal;
    uint16_t weight;
    uint32_t texcoord;           // the TEXCOORD command's word (t16 u, v)
} FighterViewCorner;

typedef struct {
    const FighterModel *model;
    int textures[MODEL_MATERIALS_MAX];
    FighterViewCorner corners[FIGHTER_VIEW_CORNERS_MAX];
    ModelVertex model_vertices[FIGHTER_VIEW_VERTICES_MAX];   // unpacked once
    ModelVertex model_normals[FIGHTER_VIEW_NORMALS_MAX];
    Vec3 vertices[FIGHTER_VIEW_VERTICES_MAX];
    uint8_t light[FIGHTER_VIEW_NORMALS_MAX];
    uint16_t colours[FIGHTER_VIEW_NORMALS_MAX];
    uint32_t texture_bytes;      // in texture VRAM, palettes included
    uint32_t pose_ticks;         // the last draw's posing and lighting (timer ticks, cpuGetTiming)
} FighterView;

// Uploads the model's textures; false when they do not fit (VRAM or the view's limits).
bool fighter_view_init(FighterView *view, const FighterModel *model);

// Frees the textures.
void fighter_view_release(FighterView *view);

void fighter_view_draw(FighterView *view, const FighterSkeleton *skeleton);

#endif
