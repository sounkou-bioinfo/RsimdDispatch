/* Staged by configure with AVX-512F/BW/VL enabled. */

#include <stddef.h>
#include <stdint.h>

#include <simde/x86/avx512.h>

#include "kernel_common.h"

size_t rsd_count_nonzero_avx512(const uint8_t *x, size_t n) {
    size_t i = 0;
    size_t acc = 0;
    const simde__m512i zero = simde_mm512_setzero_si512();

    for (; i + 64 <= n; i += 64) {
        simde__m512i v = simde_mm512_loadu_si512((const void *)(x + i));
        uint64_t eq = (uint64_t)simde_mm512_cmpeq_epi8_mask(v, zero);
        acc += 64u - rsd_popcount64(eq);
    }

    for (; i < n; ++i) {
        acc += x[i] != 0;
    }
    return acc;
}
