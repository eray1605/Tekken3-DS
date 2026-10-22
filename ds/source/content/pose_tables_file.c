#include "content/pose_tables_file.h"

#include <string.h>

#include "content/bytes.h"
#include "core/fighter/pose_solver.h"

// "T3PS", u32 version, s16 sin[5120], s16 sqrt[192], u8 euler_slots[10], 2 pad bytes,
// s16 limb_basis[9], 2 pad bytes, s32 skeleton_offsets[12], s32 limb_offsets[12]; little-endian.
#define MAGIC "T3PS"
#define VERSION 1
#define FILE_SIZE (8 + 2 * POSE_SIN_ENTRIES + 2 * POSE_SQRT_ENTRIES + 12 + 20 + 48 + 48)

static void read_vec3s(ByteReader *r, Vec3 *out, int count)
{
    for (int i = 0; i < count; i++)
        for (int k = 0; k < 3; k++)
            out[i].v[k] = byte_reader_s32(r);
}

bool pose_tables_parse(const uint8_t *data, size_t size, PoseTables *out)
{
    if (size != FILE_SIZE || memcmp(data, MAGIC, 4) != 0)
        return false;
    ByteReader r = {data, size, 4};
    if (byte_reader_u32(&r) != VERSION)
        return false;
    for (int i = 0; i < POSE_SIN_ENTRIES; i++)
        out->sin[i] = byte_reader_s16(&r);
    for (int i = 0; i < POSE_SQRT_ENTRIES; i++)
        out->sqrt[i] = byte_reader_s16(&r);
    for (int i = 0; i < POSE_EULER_JOINTS; i++) {
        out->euler_slots[i] = byte_reader_u8(&r);
        if (out->euler_slots[i] >= POSE_SLOT_COUNT)
            return false;
    }
    r.pos += 2;
    for (int i = 0; i < 9; i++)
        out->limb_basis.m[i] = byte_reader_s16(&r);
    r.pos += 2;
    read_vec3s(&r, out->skeleton_offsets, 4);
    read_vec3s(&r, out->limb_offsets, 4);
    return r.pos == size;
}
