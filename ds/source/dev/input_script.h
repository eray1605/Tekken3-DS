// A fixed key sequence in place of the DS keys, for checking the stance scene's commands on the DS
// or in an emulator without anyone at the keys: built with -DT3DS_INPUT_SCRIPT
// (make -C ds clean all T3DS_DEFINES=-DT3DS_INPUT_SCRIPT). The sequence and the state it ends in
// are listed in input_script.c.
#ifndef T3DS_DEV_INPUT_SCRIPT_H
#define T3DS_DEV_INPUT_SCRIPT_H

#include <stdint.h>

// The DS keys (keysHeld() layout) held in `frame`.
uint32_t input_script_keys(uint32_t frame);

#endif
