#include "content/spline_file.h"

#include <string.h>

#include "content/bytes.h"

// "T3SL", u32 version, u8 tables[0x660].
#define MAGIC "T3SL"
#define VERSION 1
#define HEADER_BYTES 8

bool spline_tables_parse(const uint8_t *data, size_t size, SplineTables *out)
{
    if (size != HEADER_BYTES + SPLINE_TABLE_BYTES || memcmp(data, MAGIC, 4) != 0)
        return false;
    ByteReader r = {data, size, 4};
    if (byte_reader_u32(&r) != VERSION)
        return false;
    out->data = data + HEADER_BYTES;
    return true;
}
