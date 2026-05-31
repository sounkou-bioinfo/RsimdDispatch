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
 * This header is intentionally co-located with the staged kernel sources so
 * backend implementations do not include private headers from src/. The
 * dispatch core consumes backend-provided kernel definition tables, similar
 * in spirit to R's native routine registration tables.
 *
 * Operation metadata is generated from operations.def so the enum and any
 * catalog built from it stay automatically synchronised.
 */

/* --------------------------------------------------------------------------
 * Call-frame structs — one per signature.
 * -------------------------------------------------------------------------- */

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

/* --------------------------------------------------------------------------
 * SdOperation enum — generated from operations.def.
 *
 * SD_OPERATION(UPPER_ID, lower_name, SIG_UPPER_ID, CallType)
 * -------------------------------------------------------------------------- */

typedef enum SdOperation {
#define SD_OPERATION(ID, name, sig, call_type) SD_OP_##ID,
#include "operations.def"
#undef SD_OPERATION
    SD_OP_COUNT
} SdOperation;

/* --------------------------------------------------------------------------
 * SdKernelSignature enum — identifies the call-frame ABI for a kernel.
 *
 * Generated from signatures.def via X-macro expansion.
 * SD_SIG_NONE is the explicit sentinel (value 0, unresolved slots).
 * SD_SIG_COUNT is the number of named signatures (not including NONE).
 *
 * To add a signature: add a row to signatures.def (and add the CallType
 * struct above, and a thunk macro below).
 * -------------------------------------------------------------------------- */

typedef enum SdKernelSignature {
    SD_SIG_NONE = 0,
#define SD_SIGNATURE(ID, call_type) SD_SIG_##ID,
#include "signatures.def"
#undef SD_SIGNATURE
    SD_SIG_COUNT
} SdKernelSignature;

/* --------------------------------------------------------------------------
 * Kernel definition — one row in a backend's registration table.
 * -------------------------------------------------------------------------- */

typedef void (*sd_kernel_invoke_fn)(void *call);

typedef struct SdKernelDef {
    SdOperation operation;
    SdKernelSignature signature;
    sd_kernel_invoke_fn invoke;
} SdKernelDef;

/* Sentinel row that terminates a SdKernelDef table. */
#define SD_KERNEL_DEF_END { SD_OP_COUNT, SD_SIG_NONE, NULL }

/* --------------------------------------------------------------------------
 * Table convenience macros
 *
 * SD_KERNEL(OP, SIG, thunk)
 *   One non-sentinel row in a kernel table.
 *   OP  — UPPER_ID from operations.def (SD_OP_ prefix added)
 *   SIG — SIG_UPPER_ID from operations.def (SD_SIG_ prefix added)
 *
 * SD_KERNEL_TABLE_BEGIN(backend_name)
 * SD_KERNEL_TABLE_END
 *   Declare the static sd_<backend>_kernels[] array.
 * -------------------------------------------------------------------------- */

#define SD_KERNEL(op, sig, thunk) \
    {SD_OP_##op, SD_SIG_##sig, thunk}

#define SD_KERNEL_TABLE_BEGIN(backend) \
    static const SdKernelDef sd_##backend##_kernels[] = {

#define SD_KERNEL_TABLE_END \
    SD_KERNEL_DEF_END       \
}

/* --------------------------------------------------------------------------
 * Call-frame thunk generators
 *
 * These macros define a static void thunk function that bridges the generic
 * void* call-frame ABI used by the dispatch engine to the typed function
 * signature expected by each kernel implementation.
 *
 * Usage:
 *   SD_DEFINE_RAW_COUNT_THUNK(sd_count_nonzero_avx2_invoke,
 *                             sd_count_nonzero_avx2)
 *   SD_DEFINE_F64_CONVOLVE_THUNK(sd_convolve1d_avx2_invoke,
 *                                sd_convolve1d_avx2)
 * -------------------------------------------------------------------------- */

#define SD_DEFINE_RAW_COUNT_THUNK(thunk_name, fn_name)             \
    static void thunk_name(void *call) {                           \
        SdCountNonzeroCall *args = (SdCountNonzeroCall *)call;     \
        args->result = fn_name(args->x, args->n);                  \
    }

#define SD_DEFINE_F64_CONVOLVE_THUNK(thunk_name, fn_name)          \
    static void thunk_name(void *call) {                           \
        SdConvolve1dCall *args = (SdConvolve1dCall *)call;         \
        fn_name(args->a, args->na, args->b, args->nb, args->out);  \
    }

/* --------------------------------------------------------------------------
 * Registration ABI
 * -------------------------------------------------------------------------- */

typedef struct SdDispatchBuilder SdDispatchBuilder;

void sd_register_kernel_table(SdDispatchBuilder *builder,
                              const SdKernelDef *kernels);

#ifdef __cplusplus
}
#endif

#endif
