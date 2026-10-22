// PoseSolver against the verified Python port (synthetic tables, tools/ds/pose_cases.py) and, when
// converted data and golden traces exist, against the pose traces of divmot00 and divmot99
// (remake/tests/core/test_pose_solver.gd). Every frame of an animation is replayed with the slots
// carried over, as while one move plays.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "content/bytes.h"
#include "content/pose_tables_file.h"
#include "core/fighter/joint_frame.h"
#include "core/fighter/pose_solver.h"
#include "core/math/gte.h"
#include "test.h"

#define TRACE_MAGIC "T3PT"
#define TRACE_VERSION 1

// Replays a pose trace file (export_traces.py's format); returns the frames compared or −1 when
// the file is not a pose trace.
static long replay_pose_trace(const PoseSolver *solver, const uint8_t *data, size_t size, const char *label)
{
    if (size < 12 || memcmp(data, TRACE_MAGIC, 4) != 0)
        return -1;
    ByteReader r = {data, size, 4};
    if (byte_reader_u32(&r) != TRACE_VERSION)
        return -1;
    uint32_t anims = byte_reader_u32(&r);
    long frames_total = 0;
    long mismatches = 0;
    for (uint32_t a = 0; a < anims; a++) {
        uint32_t offset = byte_reader_u32(&r);
        uint32_t frames = byte_reader_u32(&r);
        Mat3 slots[POSE_SLOT_COUNT];
        for (int s = 0; s < POSE_SLOT_COUNT; s++)
            slots[s] = FX_IDENTITY;
        for (uint32_t f = 0; f < frames; f++) {
            int32_t pose[POSE_CHANNELS];
            for (int c = 0; c < POSE_CHANNELS; c++)
                pose[c] = byte_reader_s16(&r);
            pose_solver_build(solver, pose, slots);
            frames_total++;
            for (int s = 0; s < POSE_SLOT_COUNT; s++) {
                for (int i = 0; i < 9; i++) {
                    int32_t want = byte_reader_s16(&r);
                    if (slots[s].m[i] != want) {
                        if (mismatches == 0)
                            test_fail(__FILE__, __LINE__, "%s anim %u frame %u slot %d [%d]: %d != %d", label,
                                      (unsigned)offset, (unsigned)f, s, i, (int)slots[s].m[i], (int)want);
                        mismatches++;
                    }
                }
            }
        }
    }
    EXPECT(r.pos == size, "%s: %zu trailing bytes", label, size - r.pos);
    EXPECT(mismatches == 0, "%s: %ld mismatched values in %ld frames", label, mismatches, frames_total);
    return frames_total;
}

static bool load_tables(const char *dir, const char *name, PoseTables *tables)
{
    size_t size = 0;
    uint8_t *data = test_read_file(dir, name, &size);
    if (!data)
        return false;
    bool ok = pose_tables_parse(data, size, tables);
    free(data);
    EXPECT(ok, "%s/%s does not parse", dir, name);
    return ok;
}

void test_pose_tables_file_rejects_bad_input(void)
{
    static PoseTables tables;
    static uint8_t data[16];
    memcpy(data, "T3PS\1\0\0\0", 8);
    EXPECT(!pose_tables_parse(data, sizeof(data), &tables), "a short file");
    size_t size = 0;
    uint8_t *file = test_read_file(test_paths.cases_dir, "pose_cases_tables.bin", &size);
    if (!file) {
        test_skip("no pose_cases_tables.bin (tools/ds/pose_cases.py)");
        return;
    }
    EXPECT(pose_tables_parse(file, size, &tables), "the synthetic tables");
    file[4] = 2;
    EXPECT(!pose_tables_parse(file, size, &tables), "another version");
    file[4] = 1;
    file[8 + 2 * POSE_SIN_ENTRIES + 2 * POSE_SQRT_ENTRIES] = POSE_SLOT_COUNT;
    EXPECT(!pose_tables_parse(file, size, &tables), "an Euler slot out of range");
    free(file);
}

void test_pose_solver_matches_port_cases(void)
{
    static PoseTables tables;
    if (!load_tables(test_paths.cases_dir, "pose_cases_tables.bin", &tables)) {
        test_skip("no pose_cases_tables.bin (tools/ds/pose_cases.py)");
        return;
    }
    size_t size = 0;
    uint8_t *data = test_read_file(test_paths.cases_dir, "pose_cases.bin", &size);
    if (!data) {
        test_skip("no pose_cases.bin (tools/ds/pose_cases.py)");
        return;
    }
    PoseSolver solver = {&tables, false};
    long frames = replay_pose_trace(&solver, data, size, "pose_cases");
    EXPECT(frames > 0, "pose_cases.bin has no frames");
    free(data);
}

void test_pose_solver_wide_elbow_differs(void)
{
    // Gameplay fix #8: for arm targets just beyond the minimum reach (lengths 71–73),
    // inv · 0x9204 overflows 32 bits in the original; the fix keeps the product.
    static PoseTables tables;
    if (!load_tables(test_paths.cases_dir, "pose_cases_tables.bin", &tables)) {
        test_skip("no pose_cases_tables.bin (tools/ds/pose_cases.py)");
        return;
    }
    // Every Euler angle zero: the root and chest frames are plain translations and the right
    // shoulder's rotation is the limb basis. The target is placed so that, in the shoulder's frame
    // plus the IK offset, it lies `reach` units along x.
    JointFrame root = {FX_IDENTITY, {{0, 0, 0}}};
    JointFrame chest = joint_frame_compose_offset(&root, &FX_IDENTITY, &tables.skeleton_offsets[0]);
    Mat3 shoulder_rot = fx_mul_matrix(&FX_IDENTITY, &tables.limb_basis);
    JointFrame shoulder = joint_frame_compose_offset(&chest, &shoulder_rot, &tables.skeleton_offsets[1]);
    PoseSolver original = {&tables, false};
    PoseSolver fixed = {&tables, true};
    int differing = 0;
    for (int reach = 60; reach < 90; reach++) {
        const Vec3 *off = &tables.limb_offsets[0];
        Vec3 d = gte_apply(&shoulder.rot, reach - off->v[0], -off->v[1], -off->v[2]);
        int32_t pose[POSE_CHANNELS] = {0};
        for (int k = 0; k < 3; k++)
            pose[3 + k] = shoulder.t.v[k] + d.v[k];
        Mat3 a[POSE_SLOT_COUNT];
        Mat3 b[POSE_SLOT_COUNT];
        for (int s = 0; s < POSE_SLOT_COUNT; s++)
            a[s] = b[s] = FX_IDENTITY;
        pose_solver_build(&original, pose, a);
        pose_solver_build(&fixed, pose, b);
        if (memcmp(&a[5], &b[5], sizeof(Mat3)) != 0)
            differing++;
    }
    EXPECT(differing >= 2 && differing <= 4, "%d arm lengths gave a different elbow, expected 71–73", differing);
}

void test_pose_solver_matches_traces(void)
{
    static PoseTables tables;
    char tables_dir[1024];
    snprintf(tables_dir, sizeof(tables_dir), "%s", test_paths.nitrofs);
    if (!load_tables(tables_dir, POSE_TABLES_FILE, &tables)) {
        test_skip("no converted pose tables (make convert, then tools/ds_import/convert.py)");
        return;
    }
    static const char *const BANKS[] = {"divmot00", "divmot99"};
    PoseSolver solver = {&tables, false};
    for (int b = 0; b < 2; b++) {
        char name[64];
        snprintf(name, sizeof(name), "pose_%s.bin", BANKS[b]);
        size_t size = 0;
        uint8_t *data = test_read_file(test_paths.traces, name, &size);
        if (!data) {
            test_skip("no pose traces (tools/research/export_traces.py)");
            return;
        }
        long frames = replay_pose_trace(&solver, data, size, BANKS[b]);
        EXPECT(frames > 0, "%s: not a pose trace or no frames", BANKS[b]);
        free(data);
    }
}
