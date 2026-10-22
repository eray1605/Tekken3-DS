#include "content/model_file.h"

#include <string.h>

#include "content/bytes.h"
#include "core/fighter/fighter_skeleton.h"

// "T3MD", u32 version, u16 vertices, normals, materials, batches, u32 corners; see models.py.
#define MAGIC "T3MD"
#define VERSION 1
#define HEADER_BYTES 20
#define VERTEX_BYTES 8
#define MATERIAL_HEAD_BYTES 12
#define BATCH_BYTES 12
#define CORNER_BYTES 10

static ModelVertex vertex_at(const uint8_t *p)
{
    ByteReader r = {p, VERTEX_BYTES, 0};
    ModelVertex v;
    v.joint = byte_reader_u8(&r);
    r.pos++;
    v.x = byte_reader_s16(&r);
    v.y = byte_reader_s16(&r);
    v.z = byte_reader_s16(&r);
    return v;
}

ModelVertex model_vertex(const FighterModel *model, int index)
{
    return vertex_at(model->vertices + VERTEX_BYTES * index);
}

ModelVertex model_normal(const FighterModel *model, int index)
{
    return vertex_at(model->normals + VERTEX_BYTES * index);
}

ModelBatch model_batch(const FighterModel *model, int index)
{
    ByteReader r = {model->batches + BATCH_BYTES * index, BATCH_BYTES, 0};
    ModelBatch b;
    b.material = byte_reader_u16(&r);
    b.corners = byte_reader_u8(&r);
    b.flags = byte_reader_u8(&r);
    b.first = byte_reader_u32(&r);
    b.polygons = byte_reader_u32(&r);
    return b;
}

ModelCorner model_corner(const FighterModel *model, uint32_t index)
{
    ByteReader r = {model->corners + CORNER_BYTES * index, CORNER_BYTES, 0};
    ModelCorner c;
    c.a = byte_reader_u16(&r);
    c.b = byte_reader_u16(&r);
    c.normal = byte_reader_u16(&r);
    c.weight = byte_reader_u8(&r);
    c.u = byte_reader_u8(&r);
    c.v = byte_reader_u8(&r);
    return c;
}

static bool power_of_two_side(unsigned n)
{
    return n >= 8 && n <= 1024 && (n & (n - 1)) == 0;
}

bool model_parse(const uint8_t *data, size_t size, FighterModel *out)
{
    if (size < HEADER_BYTES || memcmp(data, MAGIC, 4) != 0)
        return false;
    ByteReader r = {data, size, 4};
    if (byte_reader_u32(&r) != VERSION)
        return false;
    out->vertex_count = byte_reader_u16(&r);
    out->normal_count = byte_reader_u16(&r);
    out->material_count = byte_reader_u16(&r);
    out->batch_count = byte_reader_u16(&r);
    out->corner_count = byte_reader_u32(&r);
    if (out->material_count > MODEL_MATERIALS_MAX)
        return false;
    size_t pos = r.pos;
    out->vertices = data + pos;
    pos += (size_t)VERTEX_BYTES * out->vertex_count;
    out->normals = data + pos;
    pos += (size_t)VERTEX_BYTES * out->normal_count;
    if (pos > size)
        return false;
    for (int i = 0; i < out->vertex_count + out->normal_count; i++)
        if (vertex_at(out->vertices + VERTEX_BYTES * i).joint >= SKELETON_ALL_JOINTS)
            return false;
    for (int m = 0; m < out->material_count; m++) {
        if (pos + MATERIAL_HEAD_BYTES > size)
            return false;
        ByteReader h = {data + pos, MATERIAL_HEAD_BYTES, 0};
        ModelMaterial *mat = &out->materials[m];
        mat->depth = byte_reader_u8(&h);
        mat->flags = byte_reader_u8(&h);
        mat->width = byte_reader_u16(&h);
        mat->height = byte_reader_u16(&h);
        mat->palette_size = byte_reader_u16(&h);
        uint32_t texel_bytes = byte_reader_u32(&h);
        if ((mat->depth != 4 && mat->depth != 8) || mat->palette_size != 1u << mat->depth ||
            !power_of_two_side(mat->width) || !power_of_two_side(mat->height) ||
            texel_bytes < (uint32_t)mat->width * mat->height * mat->depth / 8)
            return false;
        pos += MATERIAL_HEAD_BYTES;
        mat->palette = data + pos;
        pos += 2u * mat->palette_size;
        mat->texels = data + pos;
        pos += texel_bytes;
        if (pos > size)
            return false;
    }
    out->batches = data + pos;
    pos += (size_t)BATCH_BYTES * out->batch_count;
    out->corners = data + pos;
    pos += (size_t)CORNER_BYTES * out->corner_count;
    if (pos != size)
        return false;
    out->polygon_count = 0;
    for (int b = 0; b < out->batch_count; b++) {
        ModelBatch batch = model_batch(out, b);
        if (batch.material >= out->material_count || (batch.corners != 3 && batch.corners != 4) ||
            batch.first > out->corner_count || batch.polygons * batch.corners > out->corner_count - batch.first)
            return false;
        const ModelMaterial *mat = &out->materials[batch.material];
        for (uint32_t c = batch.first; c < batch.first + batch.polygons * batch.corners; c++) {
            ModelCorner corner = model_corner(out, c);
            if (corner.a >= out->vertex_count || corner.b >= out->vertex_count ||
                corner.normal >= out->normal_count || corner.u >= mat->width || corner.v >= mat->height)
                return false;
        }
        out->polygon_count += batch.polygons;
    }
    return true;
}
