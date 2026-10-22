// The model file's checks on a small model built here, and the CPU posing of a converted model
// (fighter_mesh: vertices in their joints' frames, the seams' blends, the light levels) against
// tools/ds/data_cases.py on the first frames of the costume's skeleton cases.
#include <stdlib.h>
#include <string.h>

#include "content/bytes.h"
#include "content/model_file.h"
#include "core/fighter/fighter_skeleton.h"
#include "presentation/fighter_mesh.h"
#include "test.h"

// One vertex, one normal, one 8 × 8 4-bit material and one triangle; returns the size.
static size_t build_model(uint8_t *out)
{
    static const uint8_t HEAD[] = {'T', '3', 'M', 'D', 1, 0, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 3, 0, 0, 0};
    size_t n = 0;
    memcpy(out, HEAD, sizeof(HEAD));
    n += sizeof(HEAD);
    static const uint8_t POINTS[] = {5, 0, 1, 0, 2, 0, 3, 0, /* normal */ 5, 0, 0, 0x10, 0, 0, 0, 0};
    memcpy(out + n, POINTS, sizeof(POINTS));
    n += sizeof(POINTS);
    static const uint8_t MATERIAL[] = {4, 1, 8, 0, 8, 0, 16, 0, 32, 0, 0, 0};
    memcpy(out + n, MATERIAL, sizeof(MATERIAL));
    n += sizeof(MATERIAL);
    memset(out + n, 0x11, 32 + 32);   // palette, texels
    n += 64;
    static const uint8_t BATCH[] = {0, 0, 3, 0, 0, 0, 0, 0, 1, 0, 0, 0};
    memcpy(out + n, BATCH, sizeof(BATCH));
    n += sizeof(BATCH);
    for (int c = 0; c < 3; c++) {
        static const uint8_t CORNER[] = {0, 0, 0, 0, 0, 0, 0, 7, 7, 0};
        memcpy(out + n, CORNER, sizeof(CORNER));
        n += sizeof(CORNER);
    }
    return n;
}

void test_model_file_checks_its_layout(void)
{
    static uint8_t data[256];
    static FighterModel model;
    size_t size = build_model(data);
    EXPECT(model_parse(data, size, &model), "the small model");
    EXPECT_EQ(model.polygon_count, 1);
    EXPECT_EQ(model.materials[0].width, 8);
    EXPECT_EQ(model_vertex(&model, 0).joint, 5);
    EXPECT_EQ(model_vertex(&model, 0).z, 3);
    EXPECT_EQ(model_normal(&model, 0).x, 0x1000);
    EXPECT_EQ(model_corner(&model, 2).u, 7);
    EXPECT(!model_parse(data, size - 1, &model), "a short file");
    size_t corner = size - 10;
    data[corner + 7] = 8;   // u outside the 8-texel texture
    EXPECT(!model_parse(data, size, &model), "a corner outside its texture");
    data[corner + 7] = 7;
    data[corner] = 1;       // vertex 1 of 1
    EXPECT(!model_parse(data, size, &model), "a corner's vertex out of range");
    data[corner] = 0;
    data[20] = 22;          // joint 22
    EXPECT(!model_parse(data, size, &model), "a joint out of range");
    data[20] = 5;
    data[36 + 2] = 12;      // width 12
    EXPECT(!model_parse(data, size, &model), "a texture side that is no power of two");
}

static JointFrame read_frame(ByteReader *r)
{
    JointFrame f;
    for (int i = 0; i < 9; i++)
        f.rot.m[i] = byte_reader_s16(r);
    for (int k = 0; k < 3; k++)
        f.t.v[k] = byte_reader_s32(r);
    return f;
}

void test_fighter_mesh_matches_port_on_converted_data(void)
{
    size_t model_size = 0;
    uint8_t *model_data = test_read_file(test_paths.nitrofs, "models/costume_00.bin", &model_size);
    size_t size = 0;
    uint8_t *data = test_read_file(test_paths.data_dir, "mesh_costume_00.bin", &size);
    if (!model_data || !data) {
        test_skip("no converted model or mesh cases (make convert-ds, tools/ds/data_cases.py)");
        free(model_data);
        free(data);
        return;
    }
    static FighterModel model;
    EXPECT(model_parse(model_data, model_size, &model), "the converted model");
    ByteReader r = {data, size, 4};
    EXPECT(memcmp(data, "T3MS", 4) == 0 && byte_reader_u32(&r) == 1, "not a version 1 mesh case file");
    uint32_t frames = byte_reader_u32(&r);
    Vec3 light;
    for (int k = 0; k < 3; k++)
        light.v[k] = byte_reader_s32(&r);
    int ambient = byte_reader_s32(&r);
    int diffuse = byte_reader_s32(&r);
    uint32_t corners = byte_reader_u32(&r);
    uint32_t normals = byte_reader_u32(&r);
    EXPECT_EQ(corners, model.corner_count);
    EXPECT_EQ(normals, (uint32_t)model.normal_count);

    ModelVertex *vertices = malloc(sizeof(ModelVertex) * (size_t)model.vertex_count);
    ModelVertex *normal_data = malloc(sizeof(ModelVertex) * (size_t)model.normal_count);
    Vec3 *posed = malloc(sizeof(Vec3) * (size_t)model.vertex_count);
    uint8_t *levels = malloc((size_t)model.normal_count);
    for (int i = 0; i < model.vertex_count; i++)
        vertices[i] = model_vertex(&model, i);
    for (int i = 0; i < model.normal_count; i++)
        normal_data[i] = model_normal(&model, i);
    long mismatches = 0;
    long seams = 0;
    for (uint32_t f = 0; f < frames && corners == model.corner_count && r.pos < size; f++) {
        JointFrame root = read_frame(&r);
        JointFrame joints[SKELETON_ALL_JOINTS];
        for (int m = 0; m < SKELETON_ALL_JOINTS; m++)
            joints[m] = read_frame(&r);
        fighter_mesh_pose_vertices(vertices, model.vertex_count, joints, &root.t, posed);
        for (uint32_t c = 0; c < corners; c++) {
            ModelCorner corner = model_corner(&model, c);
            Vec3 got = fighter_mesh_corner(&posed[corner.a], &posed[corner.b], corner.weight);
            seams += corner.weight != 0;
            for (int k = 0; k < 3; k++) {
                int32_t want = byte_reader_s32(&r);
                if (got.v[k] != want) {
                    if (mismatches == 0)
                        test_fail(__FILE__, __LINE__, "frame %u corner %u axis %d: %d != %d", (unsigned)f,
                                  (unsigned)c, k, (int)got.v[k], (int)want);
                    mismatches++;
                }
            }
        }
        fighter_mesh_light(normal_data, model.normal_count, joints, &light, ambient, diffuse, levels);
        for (int n = 0; n < model.normal_count; n++) {
            uint8_t want = byte_reader_u8(&r);
            if (levels[n] != want) {
                if (mismatches == 0)
                    test_fail(__FILE__, __LINE__, "frame %u normal %d: level %d != %d", (unsigned)f, n, levels[n],
                              want);
                mismatches++;
            }
        }
    }
    EXPECT(frames > 0 && seams > 0, "frames %u, seam corners %ld", (unsigned)frames, seams);
    EXPECT(r.pos == size, "%zu bytes left", size - r.pos);
    EXPECT(mismatches == 0, "%ld mismatched values in %u frames", mismatches, (unsigned)frames);
    free(vertices);
    free(normal_data);
    free(posed);
    free(levels);
    free(model_data);
    free(data);
}
