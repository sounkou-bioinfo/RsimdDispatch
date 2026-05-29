/* Staged by configure with AVX2 enabled. */

#include <stddef.h>
#include <stdint.h>

#include <simde/x86/avx2.h>

#include "kernel_common.h"
#include "simd_dispatch.h"

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

void rsd_convolve1d_avx2(const double *a, size_t na, const double *b, size_t nb, double *out) {
    if (na == 0 || nb == 0) {
        return;
    }

    const size_t nab = na + nb - 1;
    size_t k = 0;
    const simde__m256d zero = simde_mm256_setzero_pd();
    for (; k + 4 <= nab; k += 4) {
        simde_mm256_storeu_pd(out + k, zero);
    }
    for (; k < nab; ++k) {
        out[k] = 0.0;
    }

    for (size_t i = 0; i < na; ++i) {
        const simde__m256d ai = simde_mm256_set1_pd(a[i]);
        size_t j = 0;
        for (; j + 4 <= nb; j += 4) {
            simde__m256d bv = simde_mm256_loadu_pd(b + j);
            simde__m256d ov = simde_mm256_loadu_pd(out + i + j);
            ov = simde_mm256_add_pd(ov, simde_mm256_mul_pd(ai, bv));
            simde_mm256_storeu_pd(out + i + j, ov);
        }
        for (; j < nb; ++j) {
            out[i + j] += a[i] * b[j];
        }
    }
}

void rsd_register_avx2(RsdDispatchBuilder *builder) {
    rsd_register_count_nonzero(builder, rsd_count_nonzero_avx2);
    rsd_register_convolve1d(builder, rsd_convolve1d_avx2);
}
