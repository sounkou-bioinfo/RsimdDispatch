#ifndef SD_KERNEL_COMMON_H
#define SD_KERNEL_COMMON_H

#include <stdint.h>

/* 32-bit popcount used by SSE2/SSE4.1/AVX2 x86 mask operations. */
static unsigned int sd_popcount32(uint32_t x) {
#if defined(__GNUC__) || defined(__clang__)
    return (unsigned int)__builtin_popcount((unsigned int)x);
#else
    unsigned int n = 0;
    while (x != 0) {
        n += (unsigned int)(x & UINT32_C(1));
        x >>= 1;
    }
    return n;
#endif
}

#endif
