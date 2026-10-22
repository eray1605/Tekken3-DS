// `rigs/costume_NN.bin`: a costume's joint layout as tools/ds_import/rigs.py writes it.
#ifndef T3DS_CONTENT_RIG_FILE_H
#define T3DS_CONTENT_RIG_FILE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/fighter/fighter_skeleton.h"

#define RIG_FILE_FORMAT "rigs/costume_%02d.bin"

// Fills `out` from the file's bytes; false when the magic, version or size is wrong or a joint's
// parent does not come before it (FighterSkeleton composes the joints in order).
bool rig_parse(const uint8_t *data, size_t size, FighterRig *out);

#endif
