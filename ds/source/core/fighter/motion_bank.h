// A motion bank (divmot*.bin) as the disc has it, read in place: the section directory, the move
// index and rows, and its animation streams (tools/research/motion.py, docs/research/formats/
// divmot-banks.md). MotionSet pairs a fighter's bank with the common bank divmot99
// (remake/content/motion_set.gd).
#ifndef T3DS_CORE_FIGHTER_MOTION_BANK_H
#define T3DS_CORE_FIGHTER_MOTION_BANK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/fighter/anim_stream.h"

#define MOTION_SECTIONS 15
#define MOTION_MOVE_SLOTS 0xFB7          // 4,023 move-index slots (section 2)
#define MOTION_ROW_BYTES 0x38
#define MOTION_COMMON_FLAG 0x40000000u   // slot entries and animation words in the common bank
#define MOTION_EMPTY_SLOT 0x3FFFFFFFu
#define MOTION_ANIM_SECTION 6

typedef struct {
    const uint8_t *data;
    size_t size;
    uint8_t type_code;
    uint16_t move_count;
    uint32_t bounds[MOTION_SECTIONS];
} MotionBank;

// A move row or an animation stream in one of the two banks.
typedef struct {
    const MotionBank *bank;  // NULL: none
    uint32_t value;          // row index or stream offset (section 6)
} MotionRef;

typedef struct {
    const MotionBank *own;
    const MotionBank *common;
} MotionSet;

// Checks the header and the section directory against the size (DivmotRelocate's layout).
bool motion_bank_parse(const uint8_t *data, size_t size, MotionBank *out);

// Whether the bank has a move index (the common bank's is empty).
bool motion_bank_has_index(const MotionBank *bank);

uint32_t motion_bank_slot_entry(const MotionBank *bank, int slot);
uint32_t motion_bank_row_word(const MotionBank *bank, uint32_t row, int word);

// The stream at a section-6 offset; false when it does not fit in the file.
bool motion_bank_anim(const MotionBank *bank, uint32_t offset, AnimStream *out);

// The move row of a slot; .bank NULL for an empty slot.
MotionRef motion_set_move_for_slot(const MotionSet *set, int slot);

// The animation stream of a move row.
MotionRef motion_set_anim_of(const MotionSet *set, MotionRef move);

// The animation of a slot's move; false when the slot is empty or its stream is not in the bank.
bool motion_set_anim_for_slot(const MotionSet *set, int slot, AnimStream *out);

#endif
