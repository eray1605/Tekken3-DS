// Animation streams of the motion banks and their sampling (tools/research/motion.py, bit-exact with
// AnimDecodePose 0x80038DA0 and AnimSplineSample 0x80038A1C).
//
// The remake bakes every frame at conversion (tools/remake_import/motion_bake.py); the DS keeps the
// banks' compressed streams in memory and samples them per frame, which costs a few hundred bytes
// per stream instead of 98 bytes per frame.
#ifndef T3DS_CORE_FIGHTER_ANIM_STREAM_H
#define T3DS_CORE_FIGHTER_ANIM_STREAM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ANIM_CHANNELS 49
#define ANIM_HEADER_BYTES 50          // u16 (frames, channel 0's count) + 48 count bytes
#define SPLINE_TABLE_BYTES 0x660      // the executable's 0x80096B00…0x80097160
#define SPLINE_TABLE_ORIGIN 0x40      // where 0x80096B40, the code's base address, lies in it

// The B-spline weight and index tables, as the executable has them.
typedef struct {
    const uint8_t *data;  // SPLINE_TABLE_BYTES
} SplineTables;

typedef struct {
    int frames;
    uint8_t counts[ANIM_CHANNELS];      // key words per channel
    const uint8_t *keys[ANIM_CHANNELS]; // each channel's little-endian s16 words
} AnimStream;

// The stream whose header starts at `offset` in `data`; false when it does not fit in `size` or
// has no frames.
bool anim_stream_parse(const uint8_t *data, size_t size, size_t offset, AnimStream *out);

// Value of one channel at 0-based `frame` (clamped to the stream), as AnimDecodePose writes it:
// already doubled.
int32_t anim_sample_channel(const AnimStream *stream, int channel, int frame, const SplineTables *tables);

void anim_sample_pose(const AnimStream *stream, int frame, const SplineTables *tables, int32_t pose[ANIM_CHANNELS]);

#endif
