#include "config.h"
#include "simd_dispatch.h"
#include "cpu_features.h"

#include <R.h>

#include <stdio.h>
#include <string.h>

extern size_t rsd_count_nonzero_scalar(const uint8_t *x, size_t n);
extern void rsd_convolve1d_scalar(const double *a, size_t na, const double *b, size_t nb, double *out);

static const RsdOps rsd_ops_scalar = {
    rsd_count_nonzero_scalar,
    rsd_convolve1d_scalar
};

#if RSD_HAVE_SSE2
extern size_t rsd_count_nonzero_sse2(const uint8_t *x, size_t n);
extern void rsd_convolve1d_sse2(const double *a, size_t na, const double *b, size_t nb, double *out);
static const RsdOps rsd_ops_sse2 = {
    rsd_count_nonzero_sse2,
    rsd_convolve1d_sse2
};
#endif

#if RSD_HAVE_SSE41
extern size_t rsd_count_nonzero_sse41(const uint8_t *x, size_t n);
extern void rsd_convolve1d_sse41(const double *a, size_t na, const double *b, size_t nb, double *out);
static const RsdOps rsd_ops_sse41 = {
    rsd_count_nonzero_sse41,
    rsd_convolve1d_sse41
};
#endif

#if RSD_HAVE_AVX2
extern size_t rsd_count_nonzero_avx2(const uint8_t *x, size_t n);
extern void rsd_convolve1d_avx2(const double *a, size_t na, const double *b, size_t nb, double *out);
static const RsdOps rsd_ops_avx2 = {
    rsd_count_nonzero_avx2,
    rsd_convolve1d_avx2
};
#endif

#if RSD_HAVE_AVX512
extern size_t rsd_count_nonzero_avx512(const uint8_t *x, size_t n);
extern void rsd_convolve1d_avx512(const double *a, size_t na, const double *b, size_t nb, double *out);
static const RsdOps rsd_ops_avx512 = {
    rsd_count_nonzero_avx512,
    rsd_convolve1d_avx512
};
#endif

#if RSD_HAVE_NEON
extern size_t rsd_count_nonzero_neon(const uint8_t *x, size_t n);
extern void rsd_convolve1d_neon(const double *a, size_t na, const double *b, size_t nb, double *out);
static const RsdOps rsd_ops_neon = {
    rsd_count_nonzero_neon,
    rsd_convolve1d_neon
};
#endif

#if RSD_HAVE_WASM_SIMD128
extern size_t rsd_count_nonzero_wasm_simd128(const uint8_t *x, size_t n);
extern void rsd_convolve1d_wasm_simd128(const double *a, size_t na, const double *b, size_t nb, double *out);
static const RsdOps rsd_ops_wasm_simd128 = {
    rsd_count_nonzero_wasm_simd128,
    rsd_convolve1d_wasm_simd128
};
#endif

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

static const RsdOps *rsd_backend_ops(const char *backend) {
    const RsdBackendEntry *entry = rsd_find_backend(backend);
    if (entry == NULL || !entry->compiled) {
        return NULL;
    }
    return entry->ops;
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
    const RsdBackendEntry *best = NULL;
    for (size_t i = 0; i < rsd_backend_count(); ++i) {
        const RsdBackendEntry *candidate = &rsd_backend_entries[i];
        if (rsd_backend_available(candidate->name) &&
            (best == NULL || candidate->priority < best->priority)) {
            best = candidate;
        }
    }
    return best != NULL ? best->name : "scalar";
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
    if (!rsd_dispatch_initialized) {
        rsd_init_dispatch();
    }
    return rsd_ops->count_nonzero(x, n);
}

void rsd_convolve1d(const double *a, size_t na, const double *b, size_t nb, double *out) {
    if (!rsd_dispatch_initialized) {
        rsd_init_dispatch();
    }
    rsd_ops->convolve1d(a, na, b, nb, out);
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
