#include "core/fighter/anim_stream.h"

#include "core/math/fx.h"

// Keys per channel are shifted by 1 (root and IK targets, channels 0–14) or 5 (angles): EXE 0x80097048.
#define WIDE_CHANNELS 15

static inline int32_t key(const uint8_t *keys, int i)
{
    return (int16_t)(uint16_t)(keys[2 * i] | keys[2 * i + 1] << 8);
}

static inline int32_t table_u8(const SplineTables *t, int64_t rel)
{
    int64_t i = rel + SPLINE_TABLE_ORIGIN;
    return i >= 0 && i < SPLINE_TABLE_BYTES ? t->data[i] : 0;
}

static inline int64_t table_u16(const SplineTables *t, int64_t rel)
{
    int64_t i = rel + SPLINE_TABLE_ORIGIN;
    return i >= 0 && i + 1 < SPLINE_TABLE_BYTES ? (int64_t)(t->data[i] | t->data[i + 1] << 8) : 0;
}

bool anim_stream_parse(const uint8_t *data, size_t size, size_t offset, AnimStream *out)
{
    if (offset + ANIM_HEADER_BYTES > size)
        return false;
    out->frames = data[offset];
    out->counts[0] = data[offset + 1];
    for (int c = 1; c < ANIM_CHANNELS; c++)
        out->counts[c] = data[offset + 1 + c];
    size_t pos = offset + ANIM_HEADER_BYTES;
    for (int c = 0; c < ANIM_CHANNELS; c++) {
        out->keys[c] = data + pos;
        pos += 2u * out->counts[c];
    }
    return out->frames > 0 && pos <= size;
}

// AnimSplineSample: the keyed channel's B-spline at frame f of n, the register names of the original.
// The segment walk stops at the last key a malformed channel has (the game would read on).
static int32_t spline_sample(const uint8_t *keys, int count, int frame, int frames, int shift, const SplineTables *tab)
{
    int64_t last = frames - 1;
    int64_t t6 = 0;
    int64_t t7 = 0;
    int64_t t0 = key(keys, 0);
    int64_t t4 = key(keys, 1);
    int ki = 2;
    int64_t t1 = t0;
    int64_t t3 = (t4 >> 5) * (1 << shift) + t1;
    t4 &= 0x1F;
    int64_t t2;
    for (;;) {
        t7 += t4 + 1;
        t2 = t3;
        if (t7 == last || ki >= count)
            break;
        t4 = key(keys, ki++);
        t3 += (t4 >> 5) * (1 << shift);
        int64_t t5 = frame - t7;
        t4 &= 0x1F;
        if (t5 <= 0)
            break;
        t6 = t7;
        t0 = t1;
        t1 = t2;
    }
    int64_t local = frame - t6;
    int64_t length = t7 - t6;
    if (local) {
        int32_t idx = length - 16 <= 0 ? table_u8(tab, length * 32 + local - 0x21)
                                       : table_u8(tab, -length * 32 - local + 0x420);
        local = (int64_t)idx << 1;
    }
    int64_t t8 = local;
    int64_t t9 = -local;
    int64_t lo;
    int64_t a1;
    int64_t v0;
    if (t6 == 0) {
        lo = table_u16(tab, t9 + 0x300) * (t3 - t1);
        a1 = table_u16(tab, t9 + 0x504);
        v0 = lo;
    } else {
        lo = table_u16(tab, t8 + 0x200) * (t0 - t1);
        if (t7 != last) {
            int64_t v1 = lo;
            lo = table_u16(tab, t9 + 0x300) * (t3 - t1);
            a1 = table_u16(tab, t9 + 0x402);
            v0 = lo + v1;
        } else {
            a1 = table_u16(tab, t9 + 0x504) + table_u16(tab, t9 + 0x300) + 0x2AAB;
            v0 = lo;
        }
    }
    v0 += a1 * (t2 - t1);
    v0 = fx_w32(v0) >> 16;
    return fx_w32(v0 + t1);
}

int32_t anim_sample_channel(const AnimStream *stream, int channel, int frame, const SplineTables *tables)
{
    int n = stream->frames;
    frame = frame < 0 ? 0 : frame > n - 1 ? n - 1 : frame;
    int f = channel & 1 ? n - (frame + 1) : frame;
    int c = stream->counts[channel];
    const uint8_t *keys = stream->keys[channel];
    int64_t v;
    if (c == 0)
        return 0;
    if (c == 1) {
        v = key(keys, 0);
    } else if (c == 2) {
        int64_t t = (int64_t)key(keys, 1) * f;
        if (n > 9)
            t >>= 5;
        v = (key(keys, 0) & 0xFFFF) + t;
    } else if (c == n) {
        v = key(keys, f);
    } else {
        v = spline_sample(keys, c, f, n, channel < WIDE_CHANNELS ? 1 : 5, tables);
    }
    return fx_s16(v * 2);
}

void anim_sample_pose(const AnimStream *stream, int frame, const SplineTables *tables, int32_t pose[ANIM_CHANNELS])
{
    for (int c = 0; c < ANIM_CHANNELS; c++)
        pose[c] = anim_sample_channel(stream, c, frame, tables);
}
