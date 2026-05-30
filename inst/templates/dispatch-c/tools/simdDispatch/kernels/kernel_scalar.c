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

static void sd_count_nonzero_scalar_invoke(void *call) {
    SdCountNonzeroCall *args = (SdCountNonzeroCall *)call;
    args->result = sd_count_nonzero_scalar(args->x, args->n);
}

static void sd_convolve1d_scalar_invoke(void *call) {
    SdConvolve1dCall *args = (SdConvolve1dCall *)call;
    sd_convolve1d_scalar(args->a, args->na, args->b, args->nb, args->out);
}

static const SdKernelDef sd_scalar_kernels[] = {
    {SD_OP_COUNT_NONZERO, SD_SIG_RAW_COUNT, sd_count_nonzero_scalar_invoke},
    {SD_OP_CONVOLVE1D, SD_SIG_F64_CONVOLVE, sd_convolve1d_scalar_invoke},
    SD_KERNEL_DEF_END
};

void sd_register_scalar(SdDispatchBuilder *builder) {
    sd_register_kernel_table(builder, sd_scalar_kernels);
}
