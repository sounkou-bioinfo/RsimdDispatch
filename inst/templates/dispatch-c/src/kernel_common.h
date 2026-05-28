#ifndef RSD_KERNEL_COMMON_H
#define RSD_KERNEL_COMMON_H

#include <stddef.h>
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

static void rsd_convolve3_scalar_range(const double *x,
                                       size_t n,
                                       const double kernel[3],
                                       double *out,
                                       size_t start) {
    size_t out_n = n >= 3 ? n - 2 : 0;
    for (size_t i = start; i < out_n; ++i) {
        out[i] = kernel[0] * x[i] + kernel[1] * x[i + 1] + kernel[2] * x[i + 2];
    }
}

#endif
