// `models/costume_NN.bin`: a costume's model as tools/ds_import/models.py writes it, read in place.
#ifndef T3DS_CONTENT_MODEL_FILE_H
#define T3DS_CONTENT_MODEL_FILE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MODEL_FILE_FORMAT "models/costume_%02d.bin"
#define MODEL_MATERIALS_MAX 48
#define MODEL_TRANSPARENT_INDEX0 1
#define MODEL_DOUBLE_SIDED 1

typedef struct {
    uint8_t joint;
    int16_t x, y, z;   // part-local
} ModelVertex;

typedef struct {
    uint8_t depth;     // bits per texel: 4 or 8
    uint8_t flags;     // MODEL_TRANSPARENT_INDEX0
    uint16_t width, height;
    uint16_t palette_size;
    const uint8_t *palette;   // little-endian u16 colours
    const uint8_t *texels;    // rows of `width` texels, 4-bit ones low nibble first
} ModelMaterial;

typedef struct {
    uint16_t material;
    uint8_t corners;   // per polygon: 3 or 4
    uint8_t flags;     // MODEL_DOUBLE_SIDED
    uint32_t first;    // corner index
    uint32_t polygons;
} ModelBatch;

typedef struct {
    uint16_t a, b;     // vertices; b == a when not a seam
    uint16_t normal;
    uint8_t weight;    // of b, 1/256
    uint8_t u, v;      // texels in the material's texture
} ModelCorner;

typedef struct {
    int vertex_count, normal_count, material_count, batch_count;
    uint32_t corner_count;
    uint32_t polygon_count;
    const uint8_t *vertices;   // 8 bytes each, read with model_vertex
    const uint8_t *normals;
    const uint8_t *batches;    // 12 bytes each
    const uint8_t *corners;    // 10 bytes each
    ModelMaterial materials[MODEL_MATERIALS_MAX];
} FighterModel;

// Checks the file's layout and every index in it; `data` must outlive the model.
bool model_parse(const uint8_t *data, size_t size, FighterModel *out);

ModelVertex model_vertex(const FighterModel *model, int index);
ModelVertex model_normal(const FighterModel *model, int index);
ModelBatch model_batch(const FighterModel *model, int index);
ModelCorner model_corner(const FighterModel *model, uint32_t index);

#endif
