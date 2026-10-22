// FighterSkeleton against the Python ports (pose.py's PoseBuildMatrices and ComposeJointOffset,
// fight_sim.root_rotation and RootUpdate's ground case) on a random rig and random inputs
// (tools/ds/core_cases.py), frames run on one skeleton so its local slots carry over.
#include <stdlib.h>
#include <string.h>

#include "content/bytes.h"
#include "content/pose_tables_file.h"
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

void test_fighter_skeleton_matches_port_cases(void)
{
    static PoseTables tables;
    size_t size = 0;
    uint8_t *data = test_read_file(test_paths.cases_dir, "pose_cases_tables.bin", &size);
    if (!data) {
        test_skip("no pose_cases_tables.bin (tools/ds/core_cases.py)");
        return;
    }
    EXPECT(pose_tables_parse(data, size, &tables), "the tables parse");
    free(data);
    data = test_read_file(test_paths.cases_dir, "skeleton_cases.bin", &size);
    if (!data) {
        test_skip("no skeleton_cases.bin (tools/ds/core_cases.py)");
        return;
    }
    ByteReader r = {data, size, 4};
    EXPECT(memcmp(data, "T3SK", 4) == 0 && byte_reader_u32(&r) == 1, "not a version 1 skeleton case file");
    static FighterRig rig;
    rig.scale = byte_reader_s32(&r);
    for (int m = 0; m < SKELETON_ALL_JOINTS; m++) {
        rig.parents[m] = (int8_t)byte_reader_u8(&r);
        r.pos += 3;
        for (int k = 0; k < 3; k++)
            rig.offsets[m].v[k] = byte_reader_s32(&r);
    }
    static FighterSkeleton skeleton;
    fighter_skeleton_init(&skeleton, &tables, &rig);
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
    EXPECT(frames > 0, "no frames");
    EXPECT(r.pos == size, "%zu bytes left", size - r.pos);
    EXPECT(mismatches == 0, "%ld mismatched joint frames in %u frames", mismatches, (unsigned)frames);
    free(data);
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
