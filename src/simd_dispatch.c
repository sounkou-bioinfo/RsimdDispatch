#include "config.h"
#include "simd_dispatch.h"
#include "cpu_features.h"

#include <R.h>

#include <stdio.h>
#include <string.h>

extern size_t rsd_count_nonzero_scalar(const uint8_t *x, size_t n);
extern void rsd_convolve1d_scalar(const double *a, size_t na, const double *b, size_t nb, double *out);

static const RsdOps rsd_ops_scalar = {
    .count_nonzero = rsd_count_nonzero_scalar,
    .convolve1d = rsd_convolve1d_scalar
};

#if RSD_HAVE_SSE2
extern size_t rsd_count_nonzero_sse2(const uint8_t *x, size_t n);
static const RsdOps rsd_ops_sse2 = {
    .count_nonzero = rsd_count_nonzero_sse2,
    .convolve1d = NULL
};
#endif

#if RSD_HAVE_SSE41
extern size_t rsd_count_nonzero_sse41(const uint8_t *x, size_t n);
static const RsdOps rsd_ops_sse41 = {
    .count_nonzero = rsd_count_nonzero_sse41,
    .convolve1d = NULL
};
#endif

#if RSD_HAVE_AVX2
extern size_t rsd_count_nonzero_avx2(const uint8_t *x, size_t n);
extern void rsd_convolve1d_avx2(const double *a, size_t na, const double *b, size_t nb, double *out);
static const RsdOps rsd_ops_avx2 = {
    .count_nonzero = rsd_count_nonzero_avx2,
    .convolve1d = rsd_convolve1d_avx2
};
#endif

#if RSD_HAVE_AVX512
extern size_t rsd_count_nonzero_avx512(const uint8_t *x, size_t n);
extern void rsd_convolve1d_avx512(const double *a, size_t na, const double *b, size_t nb, double *out);
static const RsdOps rsd_ops_avx512 = {
    .count_nonzero = rsd_count_nonzero_avx512,
    .convolve1d = rsd_convolve1d_avx512
};
#endif

#if RSD_HAVE_NEON
extern size_t rsd_count_nonzero_neon(const uint8_t *x, size_t n);
extern void rsd_convolve1d_neon(const double *a, size_t na, const double *b, size_t nb, double *out);
static const RsdOps rsd_ops_neon = {
    .count_nonzero = rsd_count_nonzero_neon,
    .convolve1d = rsd_convolve1d_neon
};
#endif

#if RSD_HAVE_WASM_SIMD128
extern size_t rsd_count_nonzero_wasm_simd128(const uint8_t *x, size_t n);
extern void rsd_convolve1d_wasm_simd128(const double *a, size_t na, const double *b, size_t nb, double *out);
static const RsdOps rsd_ops_wasm_simd128 = {
    .count_nonzero = rsd_count_nonzero_wasm_simd128,
    .convolve1d = rsd_convolve1d_wasm_simd128
};
#endif

static const char *rsd_operation_names[] = {
    "count_nonzero",
    "convolve1d"
};

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

static int rsd_ops_has_operation(const RsdOps *ops, RsdOperation operation) {
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

static int rsd_cpu_has_scalar_backend(void) {
    return 1;
}

typedef struct RsdBackendEntry {
    const char *name;
    int compiled;
    int (*supported)(void);
    const RsdOps *ops;
    int priority;
} RsdBackendEntry;

static const RsdBackendEntry rsd_backend_entries[] = {
    {"scalar", 1, rsd_cpu_has_scalar_backend, &rsd_ops_scalar, 70},
#if RSD_HAVE_SSE2
    {"sse2", 1, rsd_cpu_has_sse2, &rsd_ops_sse2, 40},
#else
    {"sse2", 0, rsd_cpu_has_sse2, NULL, 40},
#endif
#if RSD_HAVE_SSE41
    {"sse41", 1, rsd_cpu_has_sse41, &rsd_ops_sse41, 30},
#else
    {"sse41", 0, rsd_cpu_has_sse41, NULL, 30},
#endif
#if RSD_HAVE_AVX2
    {"avx2", 1, rsd_cpu_has_avx2, &rsd_ops_avx2, 20},
#else
    {"avx2", 0, rsd_cpu_has_avx2, NULL, 20},
#endif
#if RSD_HAVE_AVX512
    {"avx512", 1, rsd_cpu_has_avx512, &rsd_ops_avx512, 10},
#else
    {"avx512", 0, rsd_cpu_has_avx512, NULL, 10},
#endif
#if RSD_HAVE_NEON
    {"neon", 1, rsd_cpu_has_neon, &rsd_ops_neon, 50},
#else
    {"neon", 0, rsd_cpu_has_neon, NULL, 50},
#endif
#if RSD_HAVE_WASM_SIMD128
    {"wasm_simd128", 1, rsd_cpu_has_wasm_simd128, &rsd_ops_wasm_simd128, 60}
#else
    {"wasm_simd128", 0, rsd_cpu_has_wasm_simd128, NULL, 60}
#endif
};

static const RsdOps *rsd_ops = &rsd_ops_scalar;
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

int rsd_backend_known(const char *backend) {
    return rsd_find_backend(backend) != NULL;
}

int rsd_backend_compiled(const char *backend) {
    const RsdBackendEntry *entry = rsd_find_backend(backend);
    return entry != NULL && entry->compiled != 0;
}

int rsd_backend_cpu_supported(const char *backend) {
    const RsdBackendEntry *entry = rsd_find_backend(backend);
    return entry != NULL && entry->supported != NULL && entry->supported() != 0;
}

int rsd_backend_available(const char *backend) {
    return rsd_backend_compiled(backend) && rsd_backend_cpu_supported(backend);
}

int rsd_backend_operation_available(const char *backend, const char *operation_name) {
    const RsdBackendEntry *entry = rsd_find_backend(backend);
    RsdOperation operation;
    if (entry == NULL || !rsd_operation_from_name(operation_name, &operation)) {
        return 0;
    }
    return entry->compiled != 0 && entry->supported != NULL && entry->supported() != 0 &&
        rsd_ops_has_operation(entry->ops, operation);
}

static const RsdOps *rsd_backend_ops(const char *backend) {
    const RsdBackendEntry *entry = rsd_find_backend(backend);
    if (entry == NULL || !entry->compiled) {
        return NULL;
    }
    return entry->ops;
}

static const RsdBackendEntry *rsd_select_best_backend_entry(void) {
    const RsdBackendEntry *best = NULL;
    for (size_t i = 0; i < rsd_backend_count(); ++i) {
        const RsdBackendEntry *candidate = &rsd_backend_entries[i];
        if (rsd_backend_available(candidate->name) &&
            (best == NULL || candidate->priority < best->priority)) {
            best = candidate;
        }
    }
    return best;
}

static const RsdBackendEntry *rsd_select_best_backend_entry_for_operation(RsdOperation operation) {
    const RsdBackendEntry *best = NULL;
    for (size_t i = 0; i < rsd_backend_count(); ++i) {
        const RsdBackendEntry *candidate = &rsd_backend_entries[i];
        if (rsd_backend_available(candidate->name) &&
            rsd_ops_has_operation(candidate->ops, operation) &&
            (best == NULL || candidate->priority < best->priority)) {
            best = candidate;
        }
    }
    return best;
}

static void rsd_select_backend_unchecked(const char *backend) {
    const RsdOps *ops = rsd_backend_ops(backend);
    if (ops == NULL) {
        Rf_error("internal error: no operation table for backend '%s'", backend);
    }
    rsd_ops = ops;
    snprintf(rsd_selected_backend_buf, sizeof(rsd_selected_backend_buf), "%s", backend);
    rsd_dispatch_initialized = 1;
}

static const char *rsd_select_best_backend_name(void) {
    const RsdBackendEntry *best = rsd_select_best_backend_entry();
    return best != NULL ? best->name : "scalar";
}

static const RsdOps *rsd_ops_for_operation(RsdOperation operation, const char *operation_name) {
    if (!rsd_dispatch_initialized) {
        rsd_init_dispatch();
    }
    if (strcmp(rsd_requested_backend_buf, "auto") == 0) {
        const RsdBackendEntry *entry = rsd_select_best_backend_entry_for_operation(operation);
        if (entry == NULL || entry->ops == NULL) {
            Rf_error("operation '%s' is not available for any compiled and CPU-supported backend", operation_name);
        }
        rsd_ops = entry->ops;
        snprintf(rsd_selected_backend_buf, sizeof(rsd_selected_backend_buf), "%s", entry->name);
        return rsd_ops;
    }
    if (!rsd_ops_has_operation(rsd_ops, operation)) {
        Rf_error("operation '%s' is not available for selected backend '%s'", operation_name, rsd_selected_backend_buf);
    }
    return rsd_ops;
}

void rsd_init_dispatch(void) {
    if (!rsd_dispatch_initialized) {
        snprintf(rsd_requested_backend_buf, sizeof(rsd_requested_backend_buf), "%s", "auto");
        rsd_select_backend_unchecked(rsd_select_best_backend_name());
    }
}

void rsd_set_backend(const char *backend) {
    if (backend == NULL || backend[0] == '\0') {
        Rf_error("backend must be a non-empty string");
    }
    if (strcmp(backend, "auto") == 0) {
        snprintf(rsd_requested_backend_buf, sizeof(rsd_requested_backend_buf), "%s", "auto");
        rsd_select_backend_unchecked(rsd_select_best_backend_name());
        return;
    }
    if (!rsd_backend_known(backend)) {
        Rf_error("unknown SIMD backend '%s'", backend);
    }
    if (!rsd_backend_compiled(backend)) {
        Rf_error("SIMD backend '%s' was not compiled into this build", backend);
    }
    if (!rsd_backend_cpu_supported(backend)) {
        Rf_error("SIMD backend '%s' is not supported on this CPU/runtime", backend);
    }
    snprintf(rsd_requested_backend_buf, sizeof(rsd_requested_backend_buf), "%s", backend);
    rsd_select_backend_unchecked(backend);
}

size_t rsd_count_nonzero(const uint8_t *x, size_t n) {
    const RsdOps *ops = rsd_ops_for_operation(RSD_OP_COUNT_NONZERO, "count_nonzero");
    return ops->count_nonzero(x, n);
}

void rsd_convolve1d(const double *a, size_t na, const double *b, size_t nb, double *out) {
    const RsdOps *ops = rsd_ops_for_operation(RSD_OP_CONVOLVE1D, "convolve1d");
    ops->convolve1d(a, na, b, nb, out);
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

const char *rsd_dispatch_mode(void) {
    return "single-shlib-ops-table";
}
