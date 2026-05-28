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

void rsd_convolve3_avx512(const double *x, size_t n, const double kernel[3], double *out) {
    size_t i = 0;
    size_t out_n = n >= 3 ? n - 2 : 0;
    const simde__m512d k0 = simde_mm512_set1_pd(kernel[0]);
    const simde__m512d k1 = simde_mm512_set1_pd(kernel[1]);
    const simde__m512d k2 = simde_mm512_set1_pd(kernel[2]);

    for (; i + 8 <= out_n; i += 8) {
        simde__m512d x0 = simde_mm512_loadu_pd(x + i);
        simde__m512d x1 = simde_mm512_loadu_pd(x + i + 1);
        simde__m512d x2 = simde_mm512_loadu_pd(x + i + 2);
        simde__m512d y = simde_mm512_add_pd(
            simde_mm512_add_pd(simde_mm512_mul_pd(k0, x0), simde_mm512_mul_pd(k1, x1)),
            simde_mm512_mul_pd(k2, x2)
        );
        simde_mm512_storeu_pd(out + i, y);
    }

    rsd_convolve3_scalar_range(x, n, kernel, out, i);
}
