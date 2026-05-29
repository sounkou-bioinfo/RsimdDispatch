#ifndef RSD_KERNEL_COMMON_H
#define RSD_KERNEL_COMMON_H

#include <stdint.h>

static unsigned int rsd_popcount32(uint32_t x) {
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

static unsigned int rsd_popcount64(uint64_t x) {
#if defined(__GNUC__) || defined(__clang__)
    return (unsigned int)__builtin_popcountll((unsigned long long)x);
#else
    unsigned int n = 0;
    while (x != 0) {
        n += (unsigned int)(x & UINT64_C(1));
        x >>= 1;
    }
    return n;
#endif
}

#endif
