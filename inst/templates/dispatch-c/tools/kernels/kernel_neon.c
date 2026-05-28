/* Staged by configure for NEON-capable targets. */

#include <stddef.h>
#include <stdint.h>

#include <simde/arm/neon.h>

#include "kernel_common.h"

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

void rsd_convolve3_neon(const double *x, size_t n, const double kernel[3], double *out) {
    /* Keep the NEON template portable across both AArch64 and ARMv7 targets;
       ARMv7 NEON lacks native double-precision vector arithmetic. */
    rsd_convolve3_scalar_range(x, n, kernel, out, 0);
}
