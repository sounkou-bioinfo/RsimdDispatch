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
