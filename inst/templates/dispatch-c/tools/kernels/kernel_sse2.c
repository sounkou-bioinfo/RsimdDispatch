/* Staged by configure with SSE2 enabled. */

#include <stddef.h>
#include <stdint.h>

#include <simde/x86/sse2.h>

#include "kernel_common.h"

size_t rsd_count_nonzero_sse2(const uint8_t *x, size_t n) {
    size_t i = 0;
    size_t acc = 0;
    const simde__m128i zero = simde_mm_setzero_si128();

    for (; i + 16 <= n; i += 16) {
        simde__m128i v = simde_mm_loadu_si128((const simde__m128i *)(const void *)(x + i));
        simde__m128i eq = simde_mm_cmpeq_epi8(v, zero);
        uint32_t mask = (uint32_t)simde_mm_movemask_epi8(eq);
        acc += 16u - rsd_popcount32(mask);
    }

    for (; i < n; ++i) {
        acc += x[i] != 0;
    }
    return acc;
}

void rsd_convolve3_sse2(const double *x, size_t n, const double kernel[3], double *out) {
    size_t i = 0;
    size_t out_n = n >= 3 ? n - 2 : 0;
    const simde__m128d k0 = simde_mm_set1_pd(kernel[0]);
    const simde__m128d k1 = simde_mm_set1_pd(kernel[1]);
    const simde__m128d k2 = simde_mm_set1_pd(kernel[2]);

    for (; i + 2 <= out_n; i += 2) {
        simde__m128d x0 = simde_mm_loadu_pd(x + i);
        simde__m128d x1 = simde_mm_loadu_pd(x + i + 1);
        simde__m128d x2 = simde_mm_loadu_pd(x + i + 2);
        simde__m128d y = simde_mm_add_pd(
            simde_mm_add_pd(simde_mm_mul_pd(k0, x0), simde_mm_mul_pd(k1, x1)),
            simde_mm_mul_pd(k2, x2)
        );
        simde_mm_storeu_pd(out + i, y);
    }

    rsd_convolve3_scalar_range(x, n, kernel, out, i);
}
