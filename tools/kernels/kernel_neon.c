/* Staged by configure for NEON-capable targets. */

#include <stddef.h>
#include <stdint.h>

#include <simde/arm/neon.h>

size_t rsd_count_nonzero_neon(const uint8_t *x, size_t n) {
    size_t i = 0;
    size_t acc = 0;
    const simde_uint8x16_t zero = simde_vdupq_n_u8(0);

    for (; i + 16 <= n; i += 16) {
        simde_uint8x16_t v = simde_vld1q_u8(x + i);
        simde_uint8x16_t eq = simde_vceqq_u8(v, zero);
        uint8_t lanes[16];
        simde_vst1q_u8(lanes, eq);
        for (int j = 0; j < 16; ++j) {
            acc += lanes[j] == 0;
        }
    }

    for (; i < n; ++i) {
        acc += x[i] != 0;
    }
    return acc;
}

void rsd_convolve1d_neon(const double *a, size_t na, const double *b, size_t nb, double *out) {
    if (na == 0 || nb == 0) {
        return;
    }

    const size_t nab = na + nb - 1;
    for (size_t k = 0; k < nab; ++k) {
        out[k] = 0.0;
    }

#if defined(SIMDE_ARCH_AARCH64)
    for (size_t i = 0; i < na; ++i) {
        const simde_float64x2_t ai = simde_vdupq_n_f64(a[i]);
        size_t j = 0;
        for (; j + 2 <= nb; j += 2) {
            simde_float64x2_t bv = simde_vld1q_f64(b + j);
            simde_float64x2_t ov = simde_vld1q_f64(out + i + j);
            ov = simde_vaddq_f64(ov, simde_vmulq_f64(ai, bv));
            simde_vst1q_f64(out + i + j, ov);
        }
        for (; j < nb; ++j) {
            out[i + j] += a[i] * b[j];
        }
    }
#else
    for (size_t i = 0; i < na; ++i) {
        const double ai = a[i];
        for (size_t j = 0; j < nb; ++j) {
            out[i + j] += ai * b[j];
        }
    }
#endif
}
