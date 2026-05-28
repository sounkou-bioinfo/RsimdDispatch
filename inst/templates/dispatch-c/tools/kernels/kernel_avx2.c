/* Staged by configure with AVX2 enabled. */

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

void rsd_convolve3_avx2(const double *x, size_t n, const double kernel[3], double *out) {
    size_t i = 0;
    size_t out_n = n >= 3 ? n - 2 : 0;
    const simde__m256d k0 = simde_mm256_set1_pd(kernel[0]);
    const simde__m256d k1 = simde_mm256_set1_pd(kernel[1]);
    const simde__m256d k2 = simde_mm256_set1_pd(kernel[2]);

    for (; i + 4 <= out_n; i += 4) {
        simde__m256d x0 = simde_mm256_loadu_pd(x + i);
        simde__m256d x1 = simde_mm256_loadu_pd(x + i + 1);
        simde__m256d x2 = simde_mm256_loadu_pd(x + i + 2);
        simde__m256d y = simde_mm256_add_pd(
            simde_mm256_add_pd(simde_mm256_mul_pd(k0, x0), simde_mm256_mul_pd(k1, x1)),
            simde_mm256_mul_pd(k2, x2)
        );
        simde_mm256_storeu_pd(out + i, y);
    }

    rsd_convolve3_scalar_range(x, n, kernel, out, i);
}
