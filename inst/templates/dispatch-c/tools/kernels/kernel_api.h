#ifndef RSD_KERNEL_API_H
#define RSD_KERNEL_API_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Kernel-facing dispatch ABI.
 *
 * This header is intentionally kept with the staged kernel sources so backend
 * implementations do not include private headers from src/. The dispatch core
 * consumes backend-provided kernel definition tables, similar in spirit to R's
 * native routine registration tables.
 */

typedef enum RsdOperation {
    RSD_OP_COUNT_NONZERO = 0,
    RSD_OP_CONVOLVE1D = 1,
    RSD_OP_COUNT = 2
} RsdOperation;

typedef enum RsdKernelSignature {
    RSD_SIG_NONE = 0,
    RSD_SIG_RAW_COUNT = 1,
    RSD_SIG_F64_CONVOLVE = 2
} RsdKernelSignature;

typedef struct RsdCountNonzeroCall {
    const uint8_t *x;
    size_t n;
    size_t result;
} RsdCountNonzeroCall;

typedef struct RsdConvolve1dCall {
    const double *a;
    size_t na;
    const double *b;
    size_t nb;
    double *out;
} RsdConvolve1dCall;

typedef void (*rsd_kernel_invoke_fn)(void *call);

typedef struct RsdKernelDef {
    RsdOperation operation;
    RsdKernelSignature signature;
    rsd_kernel_invoke_fn invoke;
} RsdKernelDef;

#define RSD_KERNEL_DEF_END { RSD_OP_COUNT, RSD_SIG_NONE, NULL }

typedef struct RsdDispatchBuilder RsdDispatchBuilder;

void rsd_register_kernel_table(RsdDispatchBuilder *builder, const RsdKernelDef *kernels);

#ifdef __cplusplus
}
#endif

#endif
