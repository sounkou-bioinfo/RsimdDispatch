/* Staged by configure with AVX-512F/BW/VL enabled. */

#include <stddef.h>
#include <stdint.h>

#include <simde/x86/avx512.h>

#include "kernel_api.h"

/* 64-bit popcount for AVX-512 byte-comparison masks (one bit per lane). */
static unsigned int sd_popcount64(uint64_t x) {
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

size_t sd_count_nonzero_avx512(const uint8_t *x, size_t n) {
    size_t i = 0;
    size_t acc = 0;
    const simde__m512i zero = simde_mm512_setzero_si512();

    for (; i + 64 <= n; i += 64) {
        simde__m512i v = simde_mm512_loadu_si512((const void *)(x + i));
        uint64_t eq = (uint64_t)simde_mm512_cmpeq_epi8_mask(v, zero);
        acc += 64u - sd_popcount64(eq);
    }

    for (; i < n; ++i) {
        acc += x[i] != 0;
    }
    return acc;
}

void sd_convolve1d_avx512(const double *a, size_t na, const double *b, size_t nb, double *out) {
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

static void sd_count_nonzero_avx512_invoke(void *call) {
    SdCountNonzeroCall *args = (SdCountNonzeroCall *)call;
    args->result = sd_count_nonzero_avx512(args->x, args->n);
}

static void sd_convolve1d_avx512_invoke(void *call) {
    SdConvolve1dCall *args = (SdConvolve1dCall *)call;
    sd_convolve1d_avx512(args->a, args->na, args->b, args->nb, args->out);
}

static const SdKernelDef sd_avx512_kernels[] = {
    {SD_OP_COUNT_NONZERO, SD_SIG_RAW_COUNT, sd_count_nonzero_avx512_invoke},
    {SD_OP_CONVOLVE1D, SD_SIG_F64_CONVOLVE, sd_convolve1d_avx512_invoke},
    SD_KERNEL_DEF_END
};

void sd_register_avx512(SdDispatchBuilder *builder) {
    sd_register_kernel_table(builder, sd_avx512_kernels);
}
