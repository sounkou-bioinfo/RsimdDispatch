/*
 * example_count.c — minimal example: count non-zero bytes in a buffer
 * using the best SIMD backend available at runtime.
 *
 * Build (from tools/simdDispatch/):
 *   make examples && ./build/example_count
 */

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "simd_dispatch.h"
#include "cpu_features.h"
#include "kernels/kernel_api.h"

int main(void) {
    /* Show the selected backend. */
    sd_init_dispatch();
    printf("arch           : %s\n", sd_target_arch());
    printf("os             : %s\n", sd_target_os());
    printf("selected backend: %s\n", sd_selected_backend());
    printf("dispatch mode  : %s\n", sd_dispatch_mode());

    /* Count non-zero bytes in a small test buffer. */
    const uint8_t buf[] = {0, 1, 2, 0, 3, 0, 0, 4, 5, 0};
    const size_t n = sizeof(buf);

    SdCountNonzeroCall call = { .x = buf, .n = n, .result = 0 };
    sd_dispatch_invoke(SD_OP_COUNT_NONZERO, SD_SIG_RAW_COUNT, &call, "count_nonzero");

    printf("count_nonzero([0,1,2,0,3,0,0,4,5,0]) = %zu  (expected 5)\n", call.result);

    /* Convolve two tiny sequences: [1,2,3] * [1,0,-1] = [1, 2, 2, -2, -3] */
    const double a[] = {1.0, 2.0, 3.0};
    const double b[] = {1.0, 0.0, -1.0};
    double out[5] = {0};

    SdConvolve1dCall conv = {
        .a = a, .na = 3,
        .b = b, .nb = 3,
        .out = out
    };
    sd_dispatch_invoke(SD_OP_CONVOLVE1D, SD_SIG_F64_CONVOLVE, &conv, "convolve1d");

    printf("convolve1d([1,2,3], [1,0,-1]) = [");
    for (size_t i = 0; i < 5; ++i) {
        printf("%.0f%s", out[i], i < 4 ? ", " : "");
    }
    printf("]  (expected [1, 2, 2, -2, -3])\n");

    return 0;
}
