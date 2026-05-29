/* Staged by configure with WebAssembly SIMD128 enabled. */

#include <stddef.h>
#include <stdint.h>

#include <simde/wasm/simd128.h>

#include "kernel_common.h"

size_t rsd_count_nonzero_wasm_simd128(const uint8_t *x, size_t n) {
    size_t i = 0;
    size_t acc = 0;
    const simde_v128_t zero = simde_wasm_i8x16_splat(0);

    for (; i + 16 <= n; i += 16) {
        simde_v128_t v = simde_wasm_v128_load((const void *)(x + i));
        simde_v128_t eq = simde_wasm_i8x16_eq(v, zero);
        uint32_t mask = (uint32_t)simde_wasm_i8x16_bitmask(eq);
        acc += 16u - rsd_popcount32(mask);
    }

    for (; i < n; ++i) {
        acc += x[i] != 0;
    }
    return acc;
}

void rsd_convolve1d_wasm_simd128(const double *a, size_t na, const double *b, size_t nb, double *out) {
    if (na == 0 || nb == 0) {
        return;
    }

    const size_t nab = na + nb - 1;
    size_t k = 0;
    const simde_v128_t zero = simde_wasm_f64x2_splat(0.0);
    for (; k + 2 <= nab; k += 2) {
        simde_wasm_v128_store((void *)(out + k), zero);
    }
    for (; k < nab; ++k) {
        out[k] = 0.0;
    }

    for (size_t i = 0; i < na; ++i) {
        const simde_v128_t ai = simde_wasm_f64x2_splat(a[i]);
        size_t j = 0;
        for (; j + 2 <= nb; j += 2) {
            simde_v128_t bv = simde_wasm_v128_load((const void *)(b + j));
            simde_v128_t ov = simde_wasm_v128_load((const void *)(out + i + j));
            ov = simde_wasm_f64x2_add(ov, simde_wasm_f64x2_mul(ai, bv));
            simde_wasm_v128_store((void *)(out + i + j), ov);
        }
        for (; j < nb; ++j) {
            out[i + j] += a[i] * b[j];
        }
    }
}
