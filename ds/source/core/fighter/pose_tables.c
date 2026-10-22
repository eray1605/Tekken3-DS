#include "core/fighter/pose_tables.h"

int32_t pose_square_root0(const PoseTables *t, int32_t a)
{
    if (a <= 0)
        return 0;
    int lz = (32 - fx_bit_length(a)) & ~1;
    int32_t x = lz < 24 ? a >> (24 - lz) : (int32_t)fx_shl(a, lz - 24);
    uint32_t mant = (uint32_t)(int32_t)t->sqrt[x - POSE_SQRT_BASE];
    return (int32_t)((mant << ((31 - lz) >> 1)) >> 12);
}
