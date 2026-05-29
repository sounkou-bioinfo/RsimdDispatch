#include <stddef.h>
#include <stdint.h>

#include "simd_dispatch.h"

size_t rsd_count_nonzero_scalar(const uint8_t *x, size_t n) {
    size_t acc = 0;
    for (size_t i = 0; i < n; ++i) {
        acc += x[i] != 0;
    }
    return acc;
}

void rsd_convolve1d_scalar(const double *a, size_t na, const double *b, size_t nb, double *out) {
    if (na == 0 || nb == 0) {
        return;
    }

    const size_t nab = na + nb - 1;
    for (size_t k = 0; k < nab; ++k) {
        out[k] = 0.0;
    }

    for (size_t i = 0; i < na; ++i) {
        const double ai = a[i];
        for (size_t j = 0; j < nb; ++j) {
            out[i + j] += ai * b[j];
        }
    }
}

void rsd_register_scalar(RsdDispatchBuilder *builder) {
    rsd_register_count_nonzero(builder, rsd_count_nonzero_scalar);
    rsd_register_convolve1d(builder, rsd_convolve1d_scalar);
}
