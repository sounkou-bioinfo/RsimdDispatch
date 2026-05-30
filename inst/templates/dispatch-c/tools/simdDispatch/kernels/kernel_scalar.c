#include <stddef.h>
#include <stdint.h>
#include <limits.h>

#include "kernel_api.h"

size_t sd_count_nonzero_scalar(const uint8_t *x, size_t n) {
    size_t acc = 0;
    for (size_t i = 0; i < n; ++i) {
        acc += x[i] != 0;
    }
    return acc;
}

void sd_convolve1d_scalar(const double *a, size_t na, const double *b, size_t nb, double *out) {
    if (na == 0 || nb == 0) {
        return;
    }
    /* Precondition: na + nb - 1 must not overflow size_t.  The R API
     * enforces this before dispatch; guard defensively here for standalone
     * callers.  nb >= 1 at this point so SIZE_MAX - nb + 1 is well-defined. */
    if (na > SIZE_MAX - nb + 1) {
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

SD_DEFINE_RAW_COUNT_THUNK(sd_count_nonzero_scalar_invoke, sd_count_nonzero_scalar)
SD_DEFINE_F64_CONVOLVE_THUNK(sd_convolve1d_scalar_invoke, sd_convolve1d_scalar)

SD_KERNEL_TABLE_BEGIN(scalar)
    SD_KERNEL(COUNT_NONZERO, RAW_COUNT,    sd_count_nonzero_scalar_invoke),
    SD_KERNEL(CONVOLVE1D,    F64_CONVOLVE, sd_convolve1d_scalar_invoke),
SD_KERNEL_TABLE_END;

void sd_register_scalar(SdDispatchBuilder *builder) {
    sd_register_kernel_table(builder, sd_scalar_kernels);
}
