#include "content/characters_file.h"

#include <string.h>

#include "content/bytes.h"

// "T3CH", u32 version, u32 count, per key: s8 costume slot, u8 bank type, char name[14].
#define MAGIC "T3CH"
#define VERSION 1
#define HEADER_BYTES 12
#define ENTRY_BYTES (2 + CHARACTER_NAME_BYTES)

bool characters_parse(const uint8_t *data, size_t size, CharacterTable *out)
{
    if (size < HEADER_BYTES || memcmp(data, MAGIC, 4) != 0)
        return false;
    ByteReader r = {data, size, 4};
    if (byte_reader_u32(&r) != VERSION)
        return false;
    uint32_t count = byte_reader_u32(&r);
    if (count > CHARACTER_KEYS_MAX || size != HEADER_BYTES + ENTRY_BYTES * count)
        return false;
    out->count = (int)count;
    for (uint32_t k = 0; k < count; k++) {
        CharacterEntry *e = &out->keys[k];
        e->costume_slot = (int8_t)byte_reader_u8(&r);
        e->bank_type = byte_reader_u8(&r);
        memcpy(e->name, data + r.pos, CHARACTER_NAME_BYTES);
        e->name[CHARACTER_NAME_BYTES - 1] = '\0';
        r.pos += CHARACTER_NAME_BYTES;
    }
    return true;
}
