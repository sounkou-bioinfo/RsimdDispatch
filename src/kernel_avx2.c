/* Compile this translation unit only with AVX2 enabled. */

#include <stddef.h>
#include <stdint.h>

#include <simde/x86/avx2.h>

#include "kernel_common.h"

size_t rsd_count_nonzero_avx2(const uint8_t *x, size_t n) {
    size_t i = 0;
    size_t acc = 0;
    const simde__m256i zero = simde_mm256_setzero_si256();

    for (; i + 32 <= n; i += 32) {
        simde__m256i v = simde_mm256_loadu_si256((const simde__m256i *)(const void *)(x + i));
        simde__m256i eq = simde_mm256_cmpeq_epi8(v, zero);
        uint32_t mask = (uint32_t)simde_mm256_movemask_epi8(eq);
        acc += 32u - rsd_popcount32(mask);
    }

    for (; i < n; ++i) {
        acc += x[i] != 0;
    }
    return acc;
}
