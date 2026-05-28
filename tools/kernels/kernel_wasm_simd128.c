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

void rsd_convolve3_wasm_simd128(const double *x, size_t n, const double kernel[3], double *out) {
    size_t i = 0;
    size_t out_n = n >= 3 ? n - 2 : 0;
    const simde_v128_t k0 = simde_wasm_f64x2_splat(kernel[0]);
    const simde_v128_t k1 = simde_wasm_f64x2_splat(kernel[1]);
    const simde_v128_t k2 = simde_wasm_f64x2_splat(kernel[2]);

    for (; i + 2 <= out_n; i += 2) {
        simde_v128_t x0 = simde_wasm_v128_load((const void *)(x + i));
        simde_v128_t x1 = simde_wasm_v128_load((const void *)(x + i + 1));
        simde_v128_t x2 = simde_wasm_v128_load((const void *)(x + i + 2));
        simde_v128_t y = simde_wasm_f64x2_add(
            simde_wasm_f64x2_add(simde_wasm_f64x2_mul(k0, x0), simde_wasm_f64x2_mul(k1, x1)),
            simde_wasm_f64x2_mul(k2, x2)
        );
        simde_wasm_v128_store((void *)(out + i), y);
    }

    rsd_convolve3_scalar_range(x, n, kernel, out, i);
}
