// The C library's rand() of the PsyQ runtime (0x8007A73C, remake/core/math/c_rand.gd):
// x = x · 0x41C64E6D + 0x3039, the result bits 16–30. The state is part of the simulation state.
#ifndef T3DS_CORE_MATH_C_RAND_H
#define T3DS_CORE_MATH_C_RAND_H

#include <stdint.h>

typedef struct {
    uint32_t state;
} CRand;

static inline void c_rand_seed(CRand *r, uint32_t value)
{
    r->state = value;
}

static inline int32_t c_rand_next(CRand *r)
{
    r->state = r->state * 0x41C64E6Du + 0x3039u;
    return (int32_t)((r->state >> 16) & 0x7FFF);
}

#endif
