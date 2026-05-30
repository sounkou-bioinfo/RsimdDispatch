#include "config.h"
#include "simd_dispatch.h"
#include "cpu_features.h"

#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --------------------------------------------------------------------------
 * Pluggable error handler
 * The default writes to stderr and aborts, which is safe in any embedding
 * context.  R callers install rsd_r_error_handler() (via registration.c)
 * which redirects to Rf_error() and therefore longjmps as R expects.
 * -------------------------------------------------------------------------- */

static void rsd_default_error_handler(const char *msg) {
    fprintf(stderr, "RsimdDispatch error: %s\n", msg);
    abort();
}

static rsd_error_handler_fn rsd_error_handler = rsd_default_error_handler;

void rsd_set_error_handler(rsd_error_handler_fn handler) {
    rsd_error_handler = handler != NULL ? handler : rsd_default_error_handler;
}

#define RSD_ERROR_BUF_SIZE 512

static void rsd_raise_error(const char *fmt, ...) {
    char buf[RSD_ERROR_BUF_SIZE];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    rsd_error_handler(buf);
    /* Safety net: if the handler returns (it must not), abort to prevent UB. */
    abort();
}

/* --------------------------------------------------------------------------
 * Backend priority constants
 * Lower value = higher priority: the backend with the smallest priority number
 * wins the slot for a given operation during auto-dispatch.
 * -------------------------------------------------------------------------- */

#define RSD_PRIORITY_AVX512       10u
#define RSD_PRIORITY_AVX2         20u
#define RSD_PRIORITY_SSE41        30u
#define RSD_PRIORITY_SSE2         40u
#define RSD_PRIORITY_NEON         50u
#define RSD_PRIORITY_WASM_SIMD128 60u
#define RSD_PRIORITY_SCALAR       70u

static int rsd_cpu_has_scalar_backend(void) {
    return 1;
}

typedef void (*RsdBackendRegisterFn)(RsdDispatchBuilder *builder);

typedef struct RsdBackendEntry {
    const char *name;
    int compiled;
    int (*supported)(void);
    RsdBackendRegisterFn register_backend;
    unsigned int priority;
} RsdBackendEntry;

typedef struct RsdResolvedSlot {
    rsd_kernel_invoke_fn invoke;
    RsdKernelSignature signature;
    const char *backend;
} RsdResolvedSlot;

struct RsdDispatchBuilder {
    RsdResolvedSlot *slots;
    const RsdBackendEntry *backend;
    unsigned int best_priority[RSD_OP_COUNT];
};

extern void rsd_register_scalar(RsdDispatchBuilder *builder);
#if RSD_HAVE_SSE2
extern void rsd_register_sse2(RsdDispatchBuilder *builder);
#endif
#if RSD_HAVE_SSE41
extern void rsd_register_sse41(RsdDispatchBuilder *builder);
#endif
#if RSD_HAVE_AVX2
extern void rsd_register_avx2(RsdDispatchBuilder *builder);
#endif
#if RSD_HAVE_AVX512
extern void rsd_register_avx512(RsdDispatchBuilder *builder);
#endif
#if RSD_HAVE_NEON
extern void rsd_register_neon(RsdDispatchBuilder *builder);
#endif
#if RSD_HAVE_WASM_SIMD128
extern void rsd_register_wasm_simd128(RsdDispatchBuilder *builder);
#endif

static const RsdBackendEntry rsd_backend_entries[] = {
    {"scalar", 1, rsd_cpu_has_scalar_backend, rsd_register_scalar, RSD_PRIORITY_SCALAR},
#if RSD_HAVE_SSE2
    {"sse2", 1, rsd_cpu_has_sse2, rsd_register_sse2, RSD_PRIORITY_SSE2},
#else
    {"sse2", 0, rsd_cpu_has_sse2, NULL, RSD_PRIORITY_SSE2},
#endif
#if RSD_HAVE_SSE41
    {"sse41", 1, rsd_cpu_has_sse41, rsd_register_sse41, RSD_PRIORITY_SSE41},
#else
    {"sse41", 0, rsd_cpu_has_sse41, NULL, RSD_PRIORITY_SSE41},
#endif
#if RSD_HAVE_AVX2
    {"avx2", 1, rsd_cpu_has_avx2, rsd_register_avx2, RSD_PRIORITY_AVX2},
#else
    {"avx2", 0, rsd_cpu_has_avx2, NULL, RSD_PRIORITY_AVX2},
#endif
#if RSD_HAVE_AVX512
    {"avx512", 1, rsd_cpu_has_avx512, rsd_register_avx512, RSD_PRIORITY_AVX512},
#else
    {"avx512", 0, rsd_cpu_has_avx512, NULL, RSD_PRIORITY_AVX512},
#endif
#if RSD_HAVE_NEON
    {"neon", 1, rsd_cpu_has_neon, rsd_register_neon, RSD_PRIORITY_NEON},
#else
    {"neon", 0, rsd_cpu_has_neon, NULL, RSD_PRIORITY_NEON},
#endif
#if RSD_HAVE_WASM_SIMD128
    {"wasm_simd128", 1, rsd_cpu_has_wasm_simd128, rsd_register_wasm_simd128, RSD_PRIORITY_WASM_SIMD128}
#else
    {"wasm_simd128", 0, rsd_cpu_has_wasm_simd128, NULL, RSD_PRIORITY_WASM_SIMD128}
#endif
};

static RsdResolvedSlot rsd_active_slots[RSD_OP_COUNT] = {{0}};
static int rsd_dispatch_initialized = 0;
static char rsd_requested_backend_buf[32] = "auto";
static char rsd_selected_backend_buf[32] = "uninitialized";

size_t rsd_backend_count(void) {
    return sizeof(rsd_backend_entries) / sizeof(rsd_backend_entries[0]);
}

const char *rsd_backend_name(size_t i) {
    if (i >= rsd_backend_count()) {
        return NULL;
    }
    return rsd_backend_entries[i].name;
}

static int rsd_operation_valid(RsdOperation operation) {
    return (int)operation >= 0 && operation < RSD_OP_COUNT;
}

static const RsdBackendEntry *rsd_find_backend(const char *backend) {
    if (backend == NULL) {
        return NULL;
    }
    for (size_t i = 0; i < rsd_backend_count(); ++i) {
        if (strcmp(backend, rsd_backend_entries[i].name) == 0) {
            return &rsd_backend_entries[i];
        }
    }
    return NULL;
}

static int rsd_backend_entry_cpu_supported(const RsdBackendEntry *entry) {
    return entry != NULL && entry->supported != NULL && entry->supported() != 0;
}

static int rsd_backend_entry_available(const RsdBackendEntry *entry) {
    return entry != NULL && entry->compiled != 0 && rsd_backend_entry_cpu_supported(entry);
}

int rsd_backend_known(const char *backend) {
    return rsd_find_backend(backend) != NULL;
}

int rsd_backend_compiled(const char *backend) {
    const RsdBackendEntry *entry = rsd_find_backend(backend);
    return entry != NULL && entry->compiled != 0;
}

int rsd_backend_cpu_supported(const char *backend) {
    return rsd_backend_entry_cpu_supported(rsd_find_backend(backend));
}

int rsd_backend_available(const char *backend) {
    return rsd_backend_entry_available(rsd_find_backend(backend));
}

static void rsd_builder_init(RsdDispatchBuilder *builder, RsdResolvedSlot *slots, const RsdBackendEntry *backend) {
    builder->slots = slots;
    builder->backend = backend;
    for (size_t i = 0; i < RSD_OP_COUNT; ++i) {
        builder->best_priority[i] = UINT_MAX;
    }
}

static int rsd_builder_should_update(RsdDispatchBuilder *builder, RsdOperation operation, const RsdKernelDef *kernel) {
    if (builder == NULL || builder->slots == NULL || builder->backend == NULL || kernel == NULL) {
        return 0;
    }
    if (!rsd_operation_valid(operation) || kernel->invoke == NULL || kernel->signature == RSD_SIG_NONE) {
        return 0;
    }
    return builder->backend->priority < builder->best_priority[operation];
}

void rsd_register_kernel_table(RsdDispatchBuilder *builder, const RsdKernelDef *kernels) {
    if (builder == NULL || kernels == NULL) {
        return;
    }

    for (size_t i = 0; kernels[i].invoke != NULL; ++i) {
        RsdOperation operation = kernels[i].operation;
        if (!rsd_builder_should_update(builder, operation, &kernels[i])) {
            continue;
        }
        builder->slots[operation].invoke = kernels[i].invoke;
        builder->slots[operation].signature = kernels[i].signature;
        builder->slots[operation].backend = builder->backend->name;
        builder->best_priority[operation] = builder->backend->priority;
    }
}

static int rsd_resolved_slots_has_operation(const RsdResolvedSlot *slots, RsdOperation operation) {
    if (slots == NULL || !rsd_operation_valid(operation)) {
        return 0;
    }
    return slots[operation].invoke != NULL;
}

static const char *rsd_resolved_slots_backend_for_operation(const RsdResolvedSlot *slots, RsdOperation operation) {
    if (slots == NULL || !rsd_operation_valid(operation)) {
        return NULL;
    }
    return slots[operation].backend;
}

static void rsd_register_available_backend(RsdDispatchBuilder *builder, const RsdBackendEntry *entry) {
    if (!rsd_backend_entry_available(entry) || entry->register_backend == NULL) {
        return;
    }
    builder->backend = entry;
    entry->register_backend(builder);
}

static void rsd_build_auto_slots(RsdResolvedSlot *slots) {
    RsdDispatchBuilder builder;
    memset(slots, 0, sizeof(RsdResolvedSlot) * RSD_OP_COUNT);
    rsd_builder_init(&builder, slots, NULL);
    for (size_t i = 0; i < rsd_backend_count(); ++i) {
        rsd_register_available_backend(&builder, &rsd_backend_entries[i]);
    }
}

static void rsd_build_backend_slots(RsdResolvedSlot *slots, const RsdBackendEntry *entry) {
    RsdDispatchBuilder builder;
    memset(slots, 0, sizeof(RsdResolvedSlot) * RSD_OP_COUNT);
    rsd_builder_init(&builder, slots, entry);
    if (entry != NULL && entry->register_backend != NULL) {
        entry->register_backend(&builder);
    }
}

int rsd_backend_operation_available(const char *backend, RsdOperation operation) {
    const RsdBackendEntry *entry = rsd_find_backend(backend);
    RsdResolvedSlot slots[RSD_OP_COUNT];
    if (entry == NULL || !rsd_operation_valid(operation)) {
        return 0;
    }
    if (!rsd_backend_entry_available(entry)) {
        return 0;
    }
    rsd_build_backend_slots(slots, entry);
    return rsd_resolved_slots_has_operation(slots, operation);
}

static void rsd_update_selected_backend_summary(void) {
    const char *summary = NULL;
    int mixed = 0;

    for (size_t i = 0; i < RSD_OP_COUNT; ++i) {
        const char *backend = rsd_resolved_slots_backend_for_operation(rsd_active_slots, (RsdOperation)i);
        if (backend == NULL) {
            continue;
        }
        if (summary == NULL) {
            summary = backend;
        } else if (strcmp(summary, backend) != 0) {
            mixed = 1;
            break;
        }
    }

    if (mixed) {
        summary = "mixed";
    } else if (summary == NULL) {
        summary = "unavailable";
    }
    snprintf(rsd_selected_backend_buf, sizeof(rsd_selected_backend_buf), "%s", summary);
}

static void rsd_resolve_auto(void) {
    rsd_build_auto_slots(rsd_active_slots);
    rsd_update_selected_backend_summary();
    rsd_dispatch_initialized = 1;
}

static void rsd_resolve_backend(const RsdBackendEntry *entry) {
    rsd_build_backend_slots(rsd_active_slots, entry);
    snprintf(rsd_selected_backend_buf, sizeof(rsd_selected_backend_buf), "%s", entry->name);
    rsd_dispatch_initialized = 1;
}

static void rsd_error_unavailable_operation(const char *operation_name) {
    const char *name = operation_name != NULL ? operation_name : "<unknown>";
    if (strcmp(rsd_requested_backend_buf, "auto") == 0) {
        rsd_raise_error("operation '%s' is not available for any compiled and CPU-supported backend", name);
    }
    rsd_raise_error("operation '%s' is not available for selected backend '%s'", name, rsd_selected_backend_buf);
}

void rsd_init_dispatch(void) {
    if (!rsd_dispatch_initialized) {
        snprintf(rsd_requested_backend_buf, sizeof(rsd_requested_backend_buf), "%s", "auto");
        rsd_resolve_auto();
    }
}

void rsd_set_backend(const char *backend) {
    const RsdBackendEntry *entry;

    if (backend == NULL || backend[0] == '\0') {
        rsd_raise_error("backend must be a non-empty string");
    }
    if (strcmp(backend, "auto") == 0) {
        snprintf(rsd_requested_backend_buf, sizeof(rsd_requested_backend_buf), "%s", "auto");
        rsd_resolve_auto();
        return;
    }

    entry = rsd_find_backend(backend);
    if (entry == NULL) {
        rsd_raise_error("unknown SIMD backend '%s'", backend);
    }
    if (entry->compiled == 0) {
        rsd_raise_error("SIMD backend '%s' was not compiled into this build", backend);
    }
    if (!rsd_backend_entry_cpu_supported(entry)) {
        rsd_raise_error("SIMD backend '%s' is not supported on this CPU/runtime", backend);
    }

    snprintf(rsd_requested_backend_buf, sizeof(rsd_requested_backend_buf), "%s", backend);
    rsd_resolve_backend(entry);
}

void rsd_dispatch_invoke(RsdOperation operation, RsdKernelSignature signature, void *call, const char *operation_name) {
    if (!rsd_operation_valid(operation)) {
        rsd_raise_error("unknown operation '%s'", operation_name != NULL ? operation_name : "<unknown>");
    }
    if (!rsd_dispatch_initialized) {
        rsd_init_dispatch();
    }
    if (!rsd_resolved_slots_has_operation(rsd_active_slots, operation)) {
        rsd_error_unavailable_operation(operation_name);
    }
    if (rsd_active_slots[operation].signature != signature) {
        rsd_raise_error("internal dispatch signature mismatch for operation '%s'", operation_name != NULL ? operation_name : "<unknown>");
    }
    rsd_active_slots[operation].invoke(call);
}

const char *rsd_requested_backend(void) {
    return rsd_requested_backend_buf;
}

const char *rsd_selected_backend(void) {
    if (!rsd_dispatch_initialized) {
        rsd_init_dispatch();
    }
    return rsd_selected_backend_buf;
}

const char *rsd_operation_selected_backend(RsdOperation operation) {
    if (!rsd_dispatch_initialized) {
        rsd_init_dispatch();
    }
    return rsd_resolved_slots_backend_for_operation(rsd_active_slots, operation);
}

const char *rsd_dispatch_mode(void) {
    return "single-shlib-resolved-ops-table";
}
