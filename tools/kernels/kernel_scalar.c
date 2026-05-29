#include <stddef.h>
#include <stdint.h>

#include "kernel_api.h"

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

static void rsd_count_nonzero_scalar_invoke(void *call) {
    RsdCountNonzeroCall *args = (RsdCountNonzeroCall *)call;
    args->result = rsd_count_nonzero_scalar(args->x, args->n);
}

static void rsd_convolve1d_scalar_invoke(void *call) {
    RsdConvolve1dCall *args = (RsdConvolve1dCall *)call;
    rsd_convolve1d_scalar(args->a, args->na, args->b, args->nb, args->out);
}

static const RsdKernelDef rsd_scalar_kernels[] = {
    {RSD_OP_COUNT_NONZERO, RSD_SIG_RAW_COUNT, rsd_count_nonzero_scalar_invoke},
    {RSD_OP_CONVOLVE1D, RSD_SIG_F64_CONVOLVE, rsd_convolve1d_scalar_invoke},
    RSD_KERNEL_DEF_END
};

void rsd_register_scalar(RsdDispatchBuilder *builder) {
    rsd_register_kernel_table(builder, rsd_scalar_kernels);
}
