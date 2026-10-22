// Little-endian reads from a byte buffer, independent of the host's byte order and alignment.
// Callers check the buffer's size first; reads past the end return zero.
#ifndef T3DS_CONTENT_BYTES_H
#define T3DS_CONTENT_BYTES_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    const uint8_t *data;
    size_t size;
    size_t pos;
} ByteReader;

static inline uint8_t byte_reader_u8(ByteReader *r)
{
    return r->pos < r->size ? r->data[r->pos++] : 0;
}

static inline uint16_t byte_reader_u16(ByteReader *r)
{
    uint16_t lo = byte_reader_u8(r);
    return (uint16_t)(lo | byte_reader_u8(r) << 8);
}

static inline uint32_t byte_reader_u32(ByteReader *r)
{
    uint32_t lo = byte_reader_u16(r);
    return lo | (uint32_t)byte_reader_u16(r) << 16;
}

static inline int16_t byte_reader_s16(ByteReader *r)
{
    return (int16_t)byte_reader_u16(r);
}

static inline int32_t byte_reader_s32(ByteReader *r)
{
    return (int32_t)byte_reader_u32(r);
}

#endif
