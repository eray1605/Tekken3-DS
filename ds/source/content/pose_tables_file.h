// `tables/pose.bin`: the pose tables as tools/ds_import/pose_tables.py writes them.
#ifndef T3DS_CONTENT_POSE_TABLES_FILE_H
#define T3DS_CONTENT_POSE_TABLES_FILE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/fighter/pose_tables.h"

#define POSE_TABLES_FILE "tables/pose.bin"

// Fills `out` from the file's bytes; false when the magic, version, size or a joint slot is wrong.
bool pose_tables_parse(const uint8_t *data, size_t size, PoseTables *out);

#endif
