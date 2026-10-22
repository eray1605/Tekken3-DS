// The ROM's NitroFS: the converted data of ds/nitrofs/ (tools/ds_import/convert.py).
#ifndef T3DS_PLATFORM_FILES_H
#define T3DS_PLATFORM_FILES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Mounts NitroFS; false when the ROM has none or it cannot be read.
bool files_init(void);

// A whole file (a path inside ds/nitrofs/) into a malloc'ed buffer; NULL when missing or out of memory.
uint8_t *files_load(const char *path, size_t *size);

#endif
