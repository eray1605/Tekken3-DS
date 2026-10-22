// `tables/characters.bin`: per costume key (character · 4 + costume) the costume slot and the motion
// bank type, as tools/ds_import/characters.py writes them (FightSimulation._identity).
#ifndef T3DS_CONTENT_CHARACTERS_FILE_H
#define T3DS_CONTENT_CHARACTERS_FILE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CHARACTERS_FILE "tables/characters.bin"
#define CHARACTER_KEYS_MAX 96
#define CHARACTER_NAME_BYTES 14

typedef struct {
    int8_t costume_slot;  // the costume's model and rig, −1: none
    uint8_t bank_type;    // motion bank divmot<bank_type>
    char name[CHARACTER_NAME_BYTES];
} CharacterEntry;

typedef struct {
    int count;
    CharacterEntry keys[CHARACTER_KEYS_MAX];
} CharacterTable;

// Fills `out` from the file's bytes; false when the magic, version or size is wrong.
bool characters_parse(const uint8_t *data, size_t size, CharacterTable *out);

#endif
