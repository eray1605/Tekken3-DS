#include "core/fighter/motion_bank.h"

#define DIRECTORY 4      // u32 bounds[15] after type and move count
#define FIRST_SECTION 0x40
#define ROW_ANIM 0       // the move row's animation word

static uint32_t u32_at(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

bool motion_bank_parse(const uint8_t *data, size_t size, MotionBank *out)
{
    if (size < FIRST_SECTION || data[0] != 0)
        return false;
    out->data = data;
    out->size = size;
    out->type_code = data[1];
    out->move_count = (uint16_t)(data[2] | data[3] << 8);
    for (int i = 0; i < MOTION_SECTIONS; i++) {
        out->bounds[i] = u32_at(data + DIRECTORY + 4 * i);
        if (out->bounds[i] > size)
            return false;
    }
    if (out->bounds[0] != FIRST_SECTION || out->bounds[MOTION_SECTIONS - 1] != size)
        return false;
    // The rows, the index and the streams must lie in the file.
    if (out->bounds[1] + (size_t)out->move_count * MOTION_ROW_BYTES > size)
        return false;
    if (motion_bank_has_index(out) && out->bounds[2] + 4u * MOTION_MOVE_SLOTS > size)
        return false;
    return true;
}

bool motion_bank_has_index(const MotionBank *bank)
{
    return bank->bounds[3] > bank->bounds[2];
}

uint32_t motion_bank_slot_entry(const MotionBank *bank, int slot)
{
    if (!motion_bank_has_index(bank) || slot < 0 || slot >= MOTION_MOVE_SLOTS)
        return MOTION_EMPTY_SLOT;
    return u32_at(bank->data + bank->bounds[2] + 4 * slot);
}

uint32_t motion_bank_row_word(const MotionBank *bank, uint32_t row, int word)
{
    return u32_at(bank->data + bank->bounds[1] + row * MOTION_ROW_BYTES + 4 * word);
}

bool motion_bank_anim(const MotionBank *bank, uint32_t offset, AnimStream *out)
{
    size_t at = (size_t)bank->bounds[MOTION_ANIM_SECTION] + offset;
    if (at >= bank->size)
        return false;
    return anim_stream_parse(bank->data, bank->size, at, out);
}

MotionRef motion_set_move_for_slot(const MotionSet *set, int slot)
{
    uint32_t entry = motion_bank_slot_entry(set->own, slot);
    if (entry == MOTION_EMPTY_SLOT)
        return (MotionRef){NULL, 0};
    if (entry >= MOTION_COMMON_FLAG)
        return (MotionRef){set->common, entry - MOTION_COMMON_FLAG};
    if (entry >= set->own->move_count)
        return (MotionRef){NULL, 0};
    return (MotionRef){set->own, entry};
}

MotionRef motion_set_anim_of(const MotionSet *set, MotionRef move)
{
    uint32_t word = motion_bank_row_word(move.bank, move.value, ROW_ANIM);
    if (word >= MOTION_COMMON_FLAG)
        return (MotionRef){set->common, word - MOTION_COMMON_FLAG};
    return (MotionRef){move.bank, word};
}

bool motion_set_anim_for_slot(const MotionSet *set, int slot, AnimStream *out)
{
    MotionRef move = motion_set_move_for_slot(set, slot);
    if (!move.bank || move.value >= move.bank->move_count)
        return false;
    MotionRef anim = motion_set_anim_of(set, move);
    return motion_bank_anim(anim.bank, anim.value, out);
}
