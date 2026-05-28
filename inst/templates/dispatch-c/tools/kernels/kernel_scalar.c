#include <stddef.h>
#include <stdint.h>

#include "kernel_common.h"

size_t rsd_count_nonzero_scalar(const uint8_t *x, size_t n) {
    size_t acc = 0;
    for (size_t i = 0; i < n; ++i) {
        acc += x[i] != 0;
    }
    return acc;
}

void rsd_convolve3_scalar(const double *x, size_t n, const double kernel[3], double *out) {
    rsd_convolve3_scalar_range(x, n, kernel, out, 0);
}
