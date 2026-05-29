#include "config.h"
#include "simd_dispatch.h"
#include "cpu_features.h"

#include <R.h>

#include <limits.h>
#include <stdio.h>
#include <string.h>

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

typedef struct RsdResolvedOps {
    rsd_count_nonzero_fn count_nonzero;
    const char *count_nonzero_backend;
    rsd_convolve1d_fn convolve1d;
    const char *convolve1d_backend;
} RsdResolvedOps;

struct RsdDispatchBuilder {
    RsdResolvedOps *ops;
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
    {"scalar", 1, rsd_cpu_has_scalar_backend, rsd_register_scalar, 70},
#if RSD_HAVE_SSE2
    {"sse2", 1, rsd_cpu_has_sse2, rsd_register_sse2, 40},
#else
    {"sse2", 0, rsd_cpu_has_sse2, NULL, 40},
#endif
#if RSD_HAVE_SSE41
    {"sse41", 1, rsd_cpu_has_sse41, rsd_register_sse41, 30},
#else
    {"sse41", 0, rsd_cpu_has_sse41, NULL, 30},
#endif
#if RSD_HAVE_AVX2
    {"avx2", 1, rsd_cpu_has_avx2, rsd_register_avx2, 20},
#else
    {"avx2", 0, rsd_cpu_has_avx2, NULL, 20},
#endif
#if RSD_HAVE_AVX512
    {"avx512", 1, rsd_cpu_has_avx512, rsd_register_avx512, 10},
#else
    {"avx512", 0, rsd_cpu_has_avx512, NULL, 10},
#endif
#if RSD_HAVE_NEON
    {"neon", 1, rsd_cpu_has_neon, rsd_register_neon, 50},
#else
    {"neon", 0, rsd_cpu_has_neon, NULL, 50},
#endif
#if RSD_HAVE_WASM_SIMD128
    {"wasm_simd128", 1, rsd_cpu_has_wasm_simd128, rsd_register_wasm_simd128, 60}
#else
    {"wasm_simd128", 0, rsd_cpu_has_wasm_simd128, NULL, 60}
#endif
};

static const char *rsd_operation_names[] = {
    "count_nonzero",
    "convolve1d"
};

static RsdResolvedOps rsd_active_ops = {0};
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

size_t rsd_operation_count(void) {
    return sizeof(rsd_operation_names) / sizeof(rsd_operation_names[0]);
}

const char *rsd_operation_name(size_t i) {
    if (i >= rsd_operation_count()) {
        return NULL;
    }
    return rsd_operation_names[i];
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

static int rsd_operation_from_name(const char *name, RsdOperation *operation) {
    if (name == NULL) {
        return 0;
    }
    for (size_t i = 0; i < rsd_operation_count(); ++i) {
        if (strcmp(name, rsd_operation_names[i]) == 0) {
            *operation = (RsdOperation)i;
            return 1;
        }
    }
    return 0;
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

static void rsd_builder_init(RsdDispatchBuilder *builder, RsdResolvedOps *ops, const RsdBackendEntry *backend) {
    builder->ops = ops;
    builder->backend = backend;
    for (size_t i = 0; i < RSD_OP_COUNT; ++i) {
        builder->best_priority[i] = UINT_MAX;
    }
}

static int rsd_builder_should_update(RsdDispatchBuilder *builder, RsdOperation operation, int has_function) {
    if (builder == NULL || builder->ops == NULL || builder->backend == NULL || !has_function) {
        return 0;
    }
    if (operation >= RSD_OP_COUNT) {
        return 0;
    }
    return builder->backend->priority < builder->best_priority[operation];
}

void rsd_register_count_nonzero(RsdDispatchBuilder *builder, rsd_count_nonzero_fn fn) {
    if (!rsd_builder_should_update(builder, RSD_OP_COUNT_NONZERO, fn != NULL)) {
        return;
    }
    builder->ops->count_nonzero = fn;
    builder->ops->count_nonzero_backend = builder->backend->name;
    builder->best_priority[RSD_OP_COUNT_NONZERO] = builder->backend->priority;
}

void rsd_register_convolve1d(RsdDispatchBuilder *builder, rsd_convolve1d_fn fn) {
    if (!rsd_builder_should_update(builder, RSD_OP_CONVOLVE1D, fn != NULL)) {
        return;
    }
    builder->ops->convolve1d = fn;
    builder->ops->convolve1d_backend = builder->backend->name;
    builder->best_priority[RSD_OP_CONVOLVE1D] = builder->backend->priority;
}

static int rsd_resolved_ops_has_operation(const RsdResolvedOps *ops, RsdOperation operation) {
    if (ops == NULL) {
        return 0;
    }
    switch (operation) {
    case RSD_OP_COUNT_NONZERO:
        return ops->count_nonzero != NULL;
    case RSD_OP_CONVOLVE1D:
        return ops->convolve1d != NULL;
    default:
        return 0;
    }
}

static const char *rsd_resolved_ops_backend_for_operation(const RsdResolvedOps *ops, RsdOperation operation) {
    if (ops == NULL) {
        return NULL;
    }
    switch (operation) {
    case RSD_OP_COUNT_NONZERO:
        return ops->count_nonzero_backend;
    case RSD_OP_CONVOLVE1D:
        return ops->convolve1d_backend;
    default:
        return NULL;
    }
}

static void rsd_register_available_backend(RsdDispatchBuilder *builder, const RsdBackendEntry *entry) {
    if (!rsd_backend_entry_available(entry) || entry->register_backend == NULL) {
        return;
    }
    builder->backend = entry;
    entry->register_backend(builder);
}

static void rsd_build_auto_ops(RsdResolvedOps *ops) {
    RsdDispatchBuilder builder;
    memset(ops, 0, sizeof(*ops));
    rsd_builder_init(&builder, ops, NULL);
    for (size_t i = 0; i < rsd_backend_count(); ++i) {
        rsd_register_available_backend(&builder, &rsd_backend_entries[i]);
    }
}

static void rsd_build_backend_ops(RsdResolvedOps *ops, const RsdBackendEntry *entry) {
    RsdDispatchBuilder builder;
    memset(ops, 0, sizeof(*ops));
    rsd_builder_init(&builder, ops, entry);
    if (entry != NULL && entry->register_backend != NULL) {
        entry->register_backend(&builder);
    }
}

int rsd_backend_operation_available(const char *backend, const char *operation_name) {
    const RsdBackendEntry *entry = rsd_find_backend(backend);
    RsdOperation operation;
    RsdResolvedOps ops;
    if (entry == NULL || !rsd_operation_from_name(operation_name, &operation)) {
        return 0;
    }
    if (!rsd_backend_entry_available(entry)) {
        return 0;
    }
    rsd_build_backend_ops(&ops, entry);
    return rsd_resolved_ops_has_operation(&ops, operation);
}

static void rsd_update_selected_backend_summary(void) {
    const char *summary = NULL;
    int mixed = 0;

    for (size_t i = 0; i < rsd_operation_count(); ++i) {
        const char *backend = rsd_resolved_ops_backend_for_operation(&rsd_active_ops, (RsdOperation)i);
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
    rsd_build_auto_ops(&rsd_active_ops);
    rsd_update_selected_backend_summary();
    rsd_dispatch_initialized = 1;
}

static void rsd_resolve_backend(const RsdBackendEntry *entry) {
    rsd_build_backend_ops(&rsd_active_ops, entry);
    snprintf(rsd_selected_backend_buf, sizeof(rsd_selected_backend_buf), "%s", entry->name);
    rsd_dispatch_initialized = 1;
}

static void rsd_error_unavailable_operation(const char *operation_name) {
    if (strcmp(rsd_requested_backend_buf, "auto") == 0) {
        Rf_error("operation '%s' is not available for any compiled and CPU-supported backend", operation_name);
    }
    Rf_error("operation '%s' is not available for selected backend '%s'", operation_name, rsd_selected_backend_buf);
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
        Rf_error("backend must be a non-empty string");
    }
    if (strcmp(backend, "auto") == 0) {
        snprintf(rsd_requested_backend_buf, sizeof(rsd_requested_backend_buf), "%s", "auto");
        rsd_resolve_auto();
        return;
    }

    entry = rsd_find_backend(backend);
    if (entry == NULL) {
        Rf_error("unknown SIMD backend '%s'", backend);
    }
    if (entry->compiled == 0) {
        Rf_error("SIMD backend '%s' was not compiled into this build", backend);
    }
    if (!rsd_backend_entry_cpu_supported(entry)) {
        Rf_error("SIMD backend '%s' is not supported on this CPU/runtime", backend);
    }

    snprintf(rsd_requested_backend_buf, sizeof(rsd_requested_backend_buf), "%s", backend);
    rsd_resolve_backend(entry);
}

size_t rsd_count_nonzero(const uint8_t *x, size_t n) {
    if (!rsd_dispatch_initialized) {
        rsd_init_dispatch();
    }
    if (rsd_active_ops.count_nonzero == NULL) {
        rsd_error_unavailable_operation("count_nonzero");
    }
    return rsd_active_ops.count_nonzero(x, n);
}

void rsd_convolve1d(const double *a, size_t na, const double *b, size_t nb, double *out) {
    if (!rsd_dispatch_initialized) {
        rsd_init_dispatch();
    }
    if (rsd_active_ops.convolve1d == NULL) {
        rsd_error_unavailable_operation("convolve1d");
    }
    rsd_active_ops.convolve1d(a, na, b, nb, out);
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

const char *rsd_operation_selected_backend(const char *operation_name) {
    RsdOperation operation;
    if (!rsd_operation_from_name(operation_name, &operation)) {
        return NULL;
    }
    if (!rsd_dispatch_initialized) {
        rsd_init_dispatch();
    }
    return rsd_resolved_ops_backend_for_operation(&rsd_active_ops, operation);
}

const char *rsd_dispatch_mode(void) {
    return "single-shlib-resolved-ops-table";
}
