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

void rsd_convolve1d_avx512(const double *a, size_t na, const double *b, size_t nb, double *out) {
    if (na == 0 || nb == 0) {
        return;
    }

    const size_t nab = na + nb - 1;
    size_t k = 0;
    const simde__m512d zero = simde_mm512_setzero_pd();
    for (; k + 8 <= nab; k += 8) {
        simde_mm512_storeu_pd(out + k, zero);
    }
    for (; k < nab; ++k) {
        out[k] = 0.0;
    }

    for (size_t i = 0; i < na; ++i) {
        const simde__m512d ai = simde_mm512_set1_pd(a[i]);
        size_t j = 0;
        for (; j + 8 <= nb; j += 8) {
            simde__m512d bv = simde_mm512_loadu_pd(b + j);
            simde__m512d ov = simde_mm512_loadu_pd(out + i + j);
            ov = simde_mm512_add_pd(ov, simde_mm512_mul_pd(ai, bv));
            simde_mm512_storeu_pd(out + i + j, ov);
        }
        for (; j < nb; ++j) {
            out[i + j] += a[i] * b[j];
        }
    }
}
