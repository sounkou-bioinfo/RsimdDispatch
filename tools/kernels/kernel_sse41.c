/* Staged by configure with SSE4.1 enabled. */

#include <stddef.h>
#include <stdint.h>

#include <simde/x86/sse4.1.h>

#include "kernel_common.h"

size_t rsd_count_nonzero_sse41(const uint8_t *x, size_t n) {
    size_t i = 0;
    size_t acc = 0;
    const simde__m128i zero = simde_mm_setzero_si128();

    for (; i + 16 <= n; i += 16) {
        simde__m128i v = simde_mm_loadu_si128((const simde__m128i *)(const void *)(x + i));
        if (simde_mm_testz_si128(v, v)) {
            continue;
        }
        simde__m128i eq = simde_mm_cmpeq_epi8(v, zero);
        uint32_t mask = (uint32_t)simde_mm_movemask_epi8(eq);
        acc += 16u - rsd_popcount32(mask);
    }

    for (; i < n; ++i) {
        acc += x[i] != 0;
    }
    return acc;
}

void rsd_convolve1d_sse41(const double *a, size_t na, const double *b, size_t nb, double *out) {
    if (na == 0 || nb == 0) {
        return;
    }

    const size_t nab = na + nb - 1;
    size_t k = 0;
    const simde__m128d zero = simde_mm_setzero_pd();
    for (; k + 2 <= nab; k += 2) {
        simde_mm_storeu_pd(out + k, zero);
    }
    for (; k < nab; ++k) {
        out[k] = 0.0;
    }

    for (size_t i = 0; i < na; ++i) {
        const simde__m128d ai = simde_mm_set1_pd(a[i]);
        size_t j = 0;
        for (; j + 2 <= nb; j += 2) {
            simde__m128d bv = simde_mm_loadu_pd(b + j);
            simde__m128d ov = simde_mm_loadu_pd(out + i + j);
            ov = simde_mm_add_pd(ov, simde_mm_mul_pd(ai, bv));
            simde_mm_storeu_pd(out + i + j, ov);
        }
        for (; j < nb; ++j) {
            out[i + j] += a[i] * b[j];
        }
    }
}
