// Animation sampling against the verified Python port (tools/research/motion.py) on random streams
// and spline tables (tools/ds/core_cases.py) and, when the game is converted, every stream of the
// banks against the remake's baked poses (tools/ds/data_cases.py); the motion banks' slot lookup
// on a small bank built here (remake/content/motion_set.gd's rules).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "content/bytes.h"
#include "content/spline_file.h"
#include "core/fighter/anim_stream.h"
#include "core/fighter/motion_bank.h"
#include "test.h"

void test_anim_sampling_matches_port_cases(void)
{
    size_t size = 0;
    uint8_t *data = test_read_file(test_paths.cases_dir, "motion_cases.bin", &size);
    if (!data) {
        test_skip("no motion_cases.bin (tools/ds/core_cases.py)");
        return;
    }
    ByteReader r = {data, size, 4};
    EXPECT(memcmp(data, "T3MC", 4) == 0 && byte_reader_u32(&r) == 1, "not a version 1 motion case file");
    uint32_t table_size = byte_reader_u32(&r);
    EXPECT_EQ(table_size, SPLINE_TABLE_BYTES);
    SplineTables tables = {data + r.pos};
    r.pos += table_size;
    uint32_t streams = byte_reader_u32(&r);
    long frames_total = 0;
    long mismatches = 0;
    for (uint32_t s = 0; s < streams && r.pos < size; s++) {
        uint32_t length = byte_reader_u32(&r);
        AnimStream stream;
        bool parsed = anim_stream_parse(data + r.pos, length, 0, &stream);
        EXPECT(parsed, "stream %u does not parse", (unsigned)s);
        r.pos += length;
        if (!parsed)
            break;
        for (int f = 0; f < stream.frames; f++) {
            int32_t pose[ANIM_CHANNELS];
            anim_sample_pose(&stream, f, &tables, pose);
            frames_total++;
            for (int c = 0; c < ANIM_CHANNELS; c++) {
                int32_t want = byte_reader_s16(&r);
                if (pose[c] != want) {
                    if (mismatches == 0)
                        test_fail(__FILE__, __LINE__, "stream %u frame %d channel %d (%d keys of %d frames): %d != %d",
                                  (unsigned)s, f, c, stream.counts[c], stream.frames, (int)pose[c], (int)want);
                    mismatches++;
                }
            }
        }
    }
    EXPECT(r.pos == size, "%zu bytes left", size - r.pos);
    EXPECT(frames_total > 0, "no frames");
    EXPECT(mismatches == 0, "%ld mismatched values in %ld frames", mismatches, frames_total);
    free(data);
}

void test_anim_sampling_clamps_frames(void)
{
    // One constant channel (key 7) and one full channel (keys 1, 2, 3), three frames.
    static const uint8_t data[ANIM_HEADER_BYTES + 8] = {
        3, 1, 3, [ANIM_HEADER_BYTES] = 7, 0, 1, 0, 2, 0, 3, 0,
    };
    AnimStream stream;
    EXPECT(anim_stream_parse(data, sizeof(data), 0, &stream), "parses");
    EXPECT_EQ(stream.frames, 3);
    EXPECT_EQ(anim_sample_channel(&stream, 0, -5, NULL), 14);
    // Channel 1 is odd: it runs backwards (frame f reads key n − 1 − f), values doubled.
    EXPECT_EQ(anim_sample_channel(&stream, 1, 0, NULL), 6);
    EXPECT_EQ(anim_sample_channel(&stream, 1, 2, NULL), 2);
    EXPECT_EQ(anim_sample_channel(&stream, 1, 99, NULL), 2);
    EXPECT_EQ(anim_sample_channel(&stream, 2, 1, NULL), 0);
    EXPECT(!anim_stream_parse(data, sizeof(data) - 1, 0, &stream), "a truncated stream");
}

// A bank of `rows` move rows with a move index (unless `with_index` is false) and one stream of
// two frames at section-6 offset 0; sections 3… are empty but for the streams.
static size_t build_bank(uint8_t *out, size_t cap, int rows, bool with_index, const uint32_t *anim_words,
                         const uint32_t *slots, int slot_count)
{
    memset(out, 0, cap);
    uint32_t rows_at = 0x40;
    uint32_t index_at = rows_at + (uint32_t)rows * MOTION_ROW_BYTES;
    uint32_t index_end = index_at + (with_index ? 4u * MOTION_MOVE_SLOTS : 0);
    uint32_t anims_at = index_end;
    uint32_t end = anims_at + ANIM_HEADER_BYTES + 2;
    uint32_t bounds[MOTION_SECTIONS] = {0x40, rows_at, index_at, index_end, index_end, index_end, anims_at};
    for (int i = 7; i < MOTION_SECTIONS; i++)
        bounds[i] = end;
    out[1] = 9;
    out[2] = (uint8_t)rows;
    for (int i = 0; i < MOTION_SECTIONS; i++)
        for (int b = 0; b < 4; b++)
            out[4 + 4 * i + b] = (uint8_t)(bounds[i] >> (8 * b));
    for (int row = 0; row < rows; row++)
        for (int b = 0; b < 4; b++)
            out[rows_at + row * MOTION_ROW_BYTES + b] = (uint8_t)(anim_words[row] >> (8 * b));
    if (with_index) {
        for (int s = 0; s < MOTION_MOVE_SLOTS; s++) {
            uint32_t entry = s < slot_count ? slots[s] : MOTION_EMPTY_SLOT;
            for (int b = 0; b < 4; b++)
                out[index_at + 4 * s + b] = (uint8_t)(entry >> (8 * b));
        }
    }
    out[anims_at] = 2;   // two frames
    out[anims_at + 1] = 1;
    out[anims_at + ANIM_HEADER_BYTES] = 5;
    return end;
}

void test_motion_set_resolves_slots(void)
{
    static uint8_t own_data[0x4100];
    static uint8_t common_data[0x200];
    // Own rows: 0 → own stream 0, 1 → common stream 0. Slots: 0 → row 0, 1 → row 1,
    // 2 → common row 0 (its stream: common stream 0), 3 → empty, 4 → a row past the end.
    static const uint32_t own_anims[] = {0, MOTION_COMMON_FLAG};
    static const uint32_t own_slots[] = {0, 1, MOTION_COMMON_FLAG, MOTION_EMPTY_SLOT, 7};
    static const uint32_t common_anims[] = {0};
    size_t own_size = build_bank(own_data, sizeof(own_data), 2, true, own_anims, own_slots, 5);
    size_t common_size = build_bank(common_data, sizeof(common_data), 1, false, common_anims, NULL, 0);
    MotionBank own;
    MotionBank common;
    EXPECT(motion_bank_parse(own_data, own_size, &own), "own bank parses");
    EXPECT(motion_bank_parse(common_data, common_size, &common), "common bank parses");
    EXPECT(!motion_bank_has_index(&common), "the common bank has no index");
    MotionSet set = {&own, &common};
    AnimStream stream;
    MotionRef move = motion_set_move_for_slot(&set, 1);
    EXPECT(move.bank == &own && move.value == 1, "slot 1: own row 1");
    MotionRef anim = motion_set_anim_of(&set, move);
    EXPECT(anim.bank == &common && anim.value == 0, "row 1's stream is in the common bank");
    EXPECT(motion_set_anim_for_slot(&set, 0, &stream) && stream.frames == 2, "slot 0 has a stream");
    EXPECT(motion_set_move_for_slot(&set, 2).bank == &common, "slot 2: a common row");
    EXPECT(motion_set_anim_for_slot(&set, 2, &stream), "slot 2's stream");
    EXPECT(!motion_set_anim_for_slot(&set, 3, &stream), "slot 3 is empty");
    EXPECT(!motion_set_anim_for_slot(&set, 4, &stream), "slot 4's row is past the rows");
    EXPECT(!motion_set_anim_for_slot(&set, MOTION_MOVE_SLOTS, &stream), "a slot past the index");
    own_data[0] = 1;
    EXPECT(!motion_bank_parse(own_data, own_size, &own), "a first byte other than zero");
    own_data[0] = 0;
    EXPECT(!motion_bank_parse(own_data, own_size - 1, &own), "a directory that does not end at the size");
}

void test_spline_file_rejects_bad_input(void)
{
    static uint8_t data[8 + SPLINE_TABLE_BYTES];
    memcpy(data, "T3SL\1\0\0\0", 8);
    SplineTables tables = {NULL};
    EXPECT(spline_tables_parse(data, sizeof(data), &tables), "the right size");
    EXPECT(tables.data == data + 8, "the tables in place");
    EXPECT(!spline_tables_parse(data, sizeof(data) - 1, &tables), "a short file");
    data[0] = 'X';
    EXPECT(!spline_tables_parse(data, sizeof(data), &tables), "another magic");
    data[0] = 'T';
    data[4] = 2;
    EXPECT(!spline_tables_parse(data, sizeof(data), &tables), "another version");
}

// Every stream a bank's baked poses hold (motion_<bank>.bin), sampled from the converted bank.
static void compare_bank_with_baked(const char *bank_name, const SplineTables *tables, long *frames_total)
{
    char name[64];
    snprintf(name, sizeof(name), "motion/%s.bin", bank_name);
    size_t bank_size = 0;
    uint8_t *bank_data = test_read_file(test_paths.nitrofs, name, &bank_size);
    snprintf(name, sizeof(name), "motion_%s.bin", bank_name);
    size_t size = 0;
    uint8_t *data = test_read_file(test_paths.data_dir, name, &size);
    MotionBank bank;
    EXPECT(bank_data && motion_bank_parse(bank_data, bank_size, &bank), "%s: no converted bank", bank_name);
    EXPECT(data && memcmp(data, "T3MB", 4) == 0, "%s: no baked poses", bank_name);
    if (!bank_data || !data || memcmp(data, "T3MB", 4) != 0 || !motion_bank_parse(bank_data, bank_size, &bank)) {
        free(bank_data);
        free(data);
        return;
    }
    ByteReader r = {data, size, 4};
    EXPECT_EQ(byte_reader_u32(&r), 1);
    uint32_t anims = byte_reader_u32(&r);
    ByteReader poses = {data, size, r.pos + 8 * (size_t)anims};
    long mismatches = 0;
    for (uint32_t a = 0; a < anims && poses.pos < size; a++) {
        uint32_t offset = byte_reader_u32(&r);
        uint32_t frames = byte_reader_u32(&r);
        AnimStream stream;
        bool found = motion_bank_anim(&bank, offset, &stream);
        EXPECT(found && stream.frames == (int)frames, "%s: stream 0x%X missing or not %u frames", bank_name,
               (unsigned)offset, (unsigned)frames);
        if (!found)
            break;
        for (uint32_t f = 0; f < frames; f++) {
            int32_t pose[ANIM_CHANNELS];
            anim_sample_pose(&stream, (int)f, tables, pose);
            for (int c = 0; c < ANIM_CHANNELS; c++) {
                int32_t want = byte_reader_s16(&poses);
                if (pose[c] != want) {
                    if (mismatches == 0)
                        test_fail(__FILE__, __LINE__, "%s stream 0x%X frame %u channel %d: %d != %d", bank_name,
                                  (unsigned)offset, (unsigned)f, c, (int)pose[c], (int)want);
                    mismatches++;
                }
            }
        }
        *frames_total += frames;
    }
    EXPECT(poses.pos == size, "%s: %zu bytes of poses left", bank_name, size - poses.pos);
    EXPECT(mismatches == 0, "%s: %ld mismatched values", bank_name, mismatches);
    free(bank_data);
    free(data);
}

void test_anim_sampling_matches_baked_poses(void)
{
    size_t size = 0;
    uint8_t *file = test_read_file(test_paths.nitrofs, SPLINE_TABLES_FILE, &size);
    if (!file) {
        test_skip("no converted spline tables (make convert-ds)");
        return;
    }
    SplineTables tables;
    EXPECT(spline_tables_parse(file, size, &tables), "%s does not parse", SPLINE_TABLES_FILE);
    uint8_t *probe = test_read_file(test_paths.data_dir, "motion_divmot99.bin", &size);
    if (!probe) {
        test_skip("no cases from the converted data (tools/ds/data_cases.py)");
        free(file);
        return;
    }
    free(probe);
    long frames_total = 0;
    for (int b = 0; b <= 21; b++) {
        char bank[16];
        snprintf(bank, sizeof(bank), "divmot%02d", b == 21 ? 99 : b);
        compare_bank_with_baked(bank, &tables, &frames_total);
    }
    EXPECT(frames_total > 0, "no frames");
    printf("    %ld frames\n", frames_total);
    free(file);
}
