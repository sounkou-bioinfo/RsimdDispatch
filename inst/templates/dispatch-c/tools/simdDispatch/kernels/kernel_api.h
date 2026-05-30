#ifndef SD_KERNEL_API_H
#define SD_KERNEL_API_H

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

typedef enum SdOperation {
    SD_OP_COUNT_NONZERO = 0,
    SD_OP_CONVOLVE1D = 1,
    SD_OP_COUNT = 2
} SdOperation;

typedef enum SdKernelSignature {
    SD_SIG_NONE = 0,
    SD_SIG_RAW_COUNT = 1,
    SD_SIG_F64_CONVOLVE = 2
} SdKernelSignature;

typedef struct SdCountNonzeroCall {
    const uint8_t *x;
    size_t n;
    size_t result;
} SdCountNonzeroCall;

typedef struct SdConvolve1dCall {
    const double *a;
    size_t na;
    const double *b;
    size_t nb;
    double *out;
} SdConvolve1dCall;

typedef void (*sd_kernel_invoke_fn)(void *call);

typedef struct SdKernelDef {
    SdOperation operation;
    SdKernelSignature signature;
    sd_kernel_invoke_fn invoke;
} SdKernelDef;

#define SD_KERNEL_DEF_END { SD_OP_COUNT, SD_SIG_NONE, NULL }

typedef struct SdDispatchBuilder SdDispatchBuilder;

void sd_register_kernel_table(SdDispatchBuilder *builder, const SdKernelDef *kernels);

#ifdef __cplusplus
}
#endif

#endif
