#include "config.h"
#include "simd_dispatch.h"
#include "cpu_features.h"

#include <R.h>

#include <stdio.h>
#include <string.h>

#define RSD_DECLARE_BACKEND_OP(suffix, name, ret, args, call_args, return_stmt) extern ret rsd_##name##_##suffix args;
#define RSD_DECLARE_BACKEND(suffix) RSD_DISPATCH_OPS(RSD_DECLARE_BACKEND_OP, suffix)
#define RSD_INIT_BACKEND_OP(suffix, name, ret, args, call_args, return_stmt) rsd_##name##_##suffix,
#define RSD_DEFINE_BACKEND_OPS(suffix) static const RsdOps rsd_ops_##suffix = { RSD_DISPATCH_OPS(RSD_INIT_BACKEND_OP, suffix) };

RSD_DECLARE_BACKEND(scalar)
RSD_DEFINE_BACKEND_OPS(scalar)

#if RSD_HAVE_SSE2
RSD_DECLARE_BACKEND(sse2)
RSD_DEFINE_BACKEND_OPS(sse2)
#define RSD_OPS_SSE2 (&rsd_ops_sse2)
#else
#define RSD_OPS_SSE2 NULL
#endif

#if RSD_HAVE_SSE41
RSD_DECLARE_BACKEND(sse41)
RSD_DEFINE_BACKEND_OPS(sse41)
#define RSD_OPS_SSE41 (&rsd_ops_sse41)
#else
#define RSD_OPS_SSE41 NULL
#endif

#if RSD_HAVE_AVX2
RSD_DECLARE_BACKEND(avx2)
RSD_DEFINE_BACKEND_OPS(avx2)
#define RSD_OPS_AVX2 (&rsd_ops_avx2)
#else
#define RSD_OPS_AVX2 NULL
#endif

#if RSD_HAVE_AVX512
RSD_DECLARE_BACKEND(avx512)
RSD_DEFINE_BACKEND_OPS(avx512)
#define RSD_OPS_AVX512 (&rsd_ops_avx512)
#else
#define RSD_OPS_AVX512 NULL
#endif

#if RSD_HAVE_NEON
RSD_DECLARE_BACKEND(neon)
RSD_DEFINE_BACKEND_OPS(neon)
#define RSD_OPS_NEON (&rsd_ops_neon)
#else
#define RSD_OPS_NEON NULL
#endif

#if RSD_HAVE_WASM_SIMD128
RSD_DECLARE_BACKEND(wasm_simd128)
RSD_DEFINE_BACKEND_OPS(wasm_simd128)
#define RSD_OPS_WASM_SIMD128 (&rsd_ops_wasm_simd128)
#else
#define RSD_OPS_WASM_SIMD128 NULL
#endif

#undef RSD_DEFINE_BACKEND_OPS
#undef RSD_INIT_BACKEND_OP
#undef RSD_DECLARE_BACKEND
#undef RSD_DECLARE_BACKEND_OP

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

#define RSD_BACKENDS(X) \
    X(scalar, "scalar", 1, rsd_cpu_has_scalar_backend, &rsd_ops_scalar, 70) \
    X(sse2, "sse2", RSD_HAVE_SSE2, rsd_cpu_has_sse2, RSD_OPS_SSE2, 40) \
    X(sse41, "sse41", RSD_HAVE_SSE41, rsd_cpu_has_sse41, RSD_OPS_SSE41, 30) \
    X(avx2, "avx2", RSD_HAVE_AVX2, rsd_cpu_has_avx2, RSD_OPS_AVX2, 20) \
    X(avx512, "avx512", RSD_HAVE_AVX512, rsd_cpu_has_avx512, RSD_OPS_AVX512, 10) \
    X(neon, "neon", RSD_HAVE_NEON, rsd_cpu_has_neon, RSD_OPS_NEON, 50) \
    X(wasm_simd128, "wasm_simd128", RSD_HAVE_WASM_SIMD128, rsd_cpu_has_wasm_simd128, RSD_OPS_WASM_SIMD128, 60)

#define RSD_BACKEND_ENTRY(suffix, name, compiled, supported, ops, priority) {name, (compiled) != 0, supported, ops, priority},
static const RsdBackendEntry rsd_backend_entries[] = {
    RSD_BACKENDS(RSD_BACKEND_ENTRY)
};
#undef RSD_BACKEND_ENTRY

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

#define RSD_DEFINE_DISPATCH_WRAPPER(data, name, ret, args, call_args, return_stmt) \
    ret rsd_##name args { \
        if (!rsd_dispatch_initialized) { \
            rsd_init_dispatch(); \
        } \
        return_stmt rsd_ops->name call_args; \
    }
RSD_DISPATCH_OPS(RSD_DEFINE_DISPATCH_WRAPPER, _)
#undef RSD_DEFINE_DISPATCH_WRAPPER

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
