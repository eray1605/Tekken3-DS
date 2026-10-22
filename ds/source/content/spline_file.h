// `tables/spline.bin`: the animation streams' spline tables as tools/ds_import/spline_tables.py
// writes them, used in place.
#ifndef T3DS_CONTENT_SPLINE_FILE_H
#define T3DS_CONTENT_SPLINE_FILE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/fighter/anim_stream.h"

#define SPLINE_TABLES_FILE "tables/spline.bin"

// Points `out` at the tables inside `data`, which must outlive it; false when the magic, version
// or size is wrong.
bool spline_tables_parse(const uint8_t *data, size_t size, SplineTables *out);

#endif
