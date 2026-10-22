#include "content/rig_file.h"

#include <string.h>

#include "content/bytes.h"

// "T3RG", u32 version, s32 scale, per joint: s8 parent, 3 pad bytes, s32 offset[3]; little-endian.
#define MAGIC "T3RG"
#define VERSION 1
#define FILE_SIZE (12 + 16 * SKELETON_ALL_JOINTS)

bool rig_parse(const uint8_t *data, size_t size, FighterRig *out)
{
    if (size != FILE_SIZE || memcmp(data, MAGIC, 4) != 0)
        return false;
    ByteReader r = {data, size, 4};
    if (byte_reader_u32(&r) != VERSION)
        return false;
    out->scale = byte_reader_s32(&r);
    for (int m = 0; m < SKELETON_ALL_JOINTS; m++) {
        out->parents[m] = (int8_t)byte_reader_u8(&r);
        if (out->parents[m] < -1 || out->parents[m] >= m)
            return false;
        r.pos += 3;
        for (int k = 0; k < 3; k++)
            out->offsets[m].v[k] = byte_reader_s32(&r);
    }
    return r.pos == size;
}
