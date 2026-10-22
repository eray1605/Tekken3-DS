// FighterSkeleton against the Python ports (pose.py's PoseBuildMatrices and ComposeJointOffset,
// fight_sim.root_rotation and RootUpdate's ground case) on a random rig and random inputs
// (tools/ds/core_cases.py) and, when the game is converted, on costumes' rigs with the converted
// pose tables and baked poses (tools/ds/data_cases.py). Frames run on one skeleton so its local
// slots carry over.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "content/bytes.h"
#include "content/pose_tables_file.h"
#include "content/rig_file.h"
#include "core/fighter/fighter_skeleton.h"
#include "test.h"

static long compare_frame(const JointFrame *got, ByteReader *r, const char *what, int index, int frame, long mismatches)
{
    JointFrame want;
    for (int i = 0; i < 9; i++)
        want.rot.m[i] = byte_reader_s16(r);
    for (int k = 0; k < 3; k++)
        want.t.v[k] = byte_reader_s32(r);
    if (memcmp(got, &want, sizeof(want)) == 0)
        return mismatches;
    if (mismatches == 0)
        test_fail(__FILE__, __LINE__, "frame %d %s %d: t (%d, %d, %d), expected (%d, %d, %d)", frame, what, index,
                  (int)got->t.v[0], (int)got->t.v[1], (int)got->t.v[2], (int)want.t.v[0], (int)want.t.v[1],
                  (int)want.t.v[2]);
    return mismatches + 1;
}

static bool load_pose_tables(const char *dir, const char *name, PoseTables *tables)
{
    size_t size = 0;
    uint8_t *data = test_read_file(dir, name, &size);
    if (!data)
        return false;
    bool ok = pose_tables_parse(data, size, tables);
    EXPECT(ok, "%s/%s does not parse", dir, name);
    free(data);
    return ok;
}

// Replays a skeleton case file (core_cases.skeleton_case_file) on one skeleton; its rig into `rig`.
static void replay_skeleton_cases(const PoseTables *tables, const uint8_t *data, size_t size, const char *label,
                                  FighterRig *rig)
{
    ByteReader r = {data, size, 4};
    EXPECT(memcmp(data, "T3SK", 4) == 0 && byte_reader_u32(&r) == 1, "%s: not a version 1 skeleton case file",
           label);
    rig->scale = byte_reader_s32(&r);
    for (int m = 0; m < SKELETON_ALL_JOINTS; m++) {
        rig->parents[m] = (int8_t)byte_reader_u8(&r);
        r.pos += 3;
        for (int k = 0; k < 3; k++)
            rig->offsets[m].v[k] = byte_reader_s32(&r);
    }
    static FighterSkeleton skeleton;
    fighter_skeleton_init(&skeleton, tables, rig);
    uint32_t frames = byte_reader_u32(&r);
    long mismatches = 0;
    for (uint32_t f = 0; f < frames && r.pos < size; f++) {
        int32_t pose[POSE_CHANNELS];
        for (int c = 0; c < POSE_CHANNELS; c++)
            pose[c] = byte_reader_s16(&r);
        Vec3 anchor;
        for (int k = 0; k < 3; k++)
            anchor.v[k] = byte_reader_s32(&r);
        int32_t facing = byte_reader_s32(&r);
        int32_t heading = byte_reader_s32(&r);
        fighter_skeleton_update(&skeleton, pose, &anchor, facing, heading);
        mismatches = compare_frame(&skeleton.root, &r, "root", 0, (int)f, mismatches);
        for (int m = 0; m < SKELETON_ALL_JOINTS; m++)
            mismatches = compare_frame(&skeleton.joints[m], &r, "joint", m, (int)f, mismatches);
    }
    EXPECT(frames > 0, "%s: no frames", label);
    EXPECT(r.pos == size, "%s: %zu bytes left", label, size - r.pos);
    EXPECT(mismatches == 0, "%s: %ld mismatched joint frames in %u frames", label, mismatches, (unsigned)frames);
}

void test_fighter_skeleton_matches_port_cases(void)
{
    static PoseTables tables;
    if (!load_pose_tables(test_paths.cases_dir, "pose_cases_tables.bin", &tables)) {
        test_skip("no pose_cases_tables.bin (tools/ds/core_cases.py)");
        return;
    }
    size_t size = 0;
    uint8_t *data = test_read_file(test_paths.cases_dir, "skeleton_cases.bin", &size);
    if (!data) {
        test_skip("no skeleton_cases.bin (tools/ds/core_cases.py)");
        return;
    }
    static FighterRig rig;
    replay_skeleton_cases(&tables, data, size, "skeleton_cases", &rig);
    free(data);
}

void test_rig_file_rejects_bad_input(void)
{
    // A chain: every joint's parent is the one before it.
    static uint8_t data[12 + 16 * SKELETON_ALL_JOINTS];
    memcpy(data, "T3RG\1\0\0\0\0\x10\0\0", 12);
    for (int m = 0; m < SKELETON_ALL_JOINTS; m++) {
        data[12 + 16 * m] = (uint8_t)(m - 1);
        data[12 + 16 * m + 4] = (uint8_t)m;
    }
    static FighterRig rig;
    EXPECT(rig_parse(data, sizeof(data), &rig), "a chain");
    EXPECT_EQ(rig.scale, FX_ONE);
    EXPECT_EQ(rig.parents[0], -1);
    EXPECT_EQ(rig.parents[21], 20);
    EXPECT_EQ(rig.offsets[21].v[0], 21);
    EXPECT(!rig_parse(data, sizeof(data) - 1, &rig), "a short file");
    data[4] = 2;
    EXPECT(!rig_parse(data, sizeof(data), &rig), "another version");
    data[4] = 1;
    data[12 + 16 * 5] = 5;
    EXPECT(!rig_parse(data, sizeof(data), &rig), "a joint its own parent");
    data[12 + 16 * 5] = (uint8_t)-2;
    EXPECT(!rig_parse(data, sizeof(data), &rig), "a parent below −1");
}

void test_fighter_skeleton_matches_port_on_converted_data(void)
{
    static PoseTables tables;
    if (!load_pose_tables(test_paths.nitrofs, POSE_TABLES_FILE, &tables)) {
        test_skip("no converted pose tables (make convert-ds)");
        return;
    }
    static const int COSTUMES[] = {42, 0, 6};   // tools/ds/data_cases.py SKELETONS
    for (size_t c = 0; c < sizeof(COSTUMES) / sizeof(COSTUMES[0]); c++) {
        char name[64];
        snprintf(name, sizeof(name), "skeleton_costume_%02d.bin", COSTUMES[c]);
        size_t size = 0;
        uint8_t *data = test_read_file(test_paths.data_dir, name, &size);
        if (!data) {
            test_skip("no cases from the converted data (tools/ds/data_cases.py)");
            return;
        }
        static FighterRig rig;
        replay_skeleton_cases(&tables, data, size, name, &rig);
        free(data);
        // The converted rig file holds the rig the cases were made with.
        snprintf(name, sizeof(name), RIG_FILE_FORMAT, COSTUMES[c]);
        data = test_read_file(test_paths.nitrofs, name, &size);
        static FighterRig converted;
        EXPECT(data && rig_parse(data, size, &converted), "%s missing or bad", name);
        EXPECT(data && memcmp(&rig, &converted, sizeof(rig)) == 0, "%s differs from the cases' rig", name);
        free(data);
    }
}

void test_fighter_rotation_xyz_identity_and_quarter_turn(void)
{
    // A 4.12 sine table: sin and cos of 0 and of a quarter turn are exact.
    static PoseTables tables;
    for (int i = 0; i < POSE_SIN_ENTRIES; i++)
        tables.sin[i] = 0;
    tables.sin[0] = 0;
    tables.sin[POSE_COS_OFFSET] = FX_ONE;       // cos 0
    tables.sin[0x400] = FX_ONE;                 // sin quarter
    tables.sin[POSE_COS_OFFSET + 0x400] = 0;    // cos quarter
    Mat3 identity = fighter_rotation_xyz(&tables, 0, 0, 0);
    EXPECT(memcmp(&identity, &FX_IDENTITY, sizeof(Mat3)) == 0, "no angles: identity");
    // A quarter turn about y (0x4000 in 16-bit units): x → −z, z → x (row-major R, column vectors).
    Mat3 y = fighter_rotation_xyz(&tables, 0, 0x4000, 0);
    static const Mat3 WANT = {{0, 0, FX_ONE, 0, FX_ONE, 0, -FX_ONE, 0, 0}};
    EXPECT(memcmp(&y, &WANT, sizeof(Mat3)) == 0, "a quarter turn about y: [%d %d %d / %d %d %d / %d %d %d]",
           (int)y.m[0], (int)y.m[1], (int)y.m[2], (int)y.m[3], (int)y.m[4], (int)y.m[5], (int)y.m[6], (int)y.m[7],
           (int)y.m[8]);
}
