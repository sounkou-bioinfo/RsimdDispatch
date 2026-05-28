#include "config.h"
#include "simd_dispatch.h"
#include "cpu_features.h"

#include <R.h>

#include <stdio.h>
#include <string.h>

extern size_t rsd_count_nonzero_scalar(const uint8_t *x, size_t n);
extern void rsd_convolve3_scalar(const double *x, size_t n, const double kernel[3], double *out);

#if RSD_HAVE_SSE2
extern size_t rsd_count_nonzero_sse2(const uint8_t *x, size_t n);
extern void rsd_convolve3_sse2(const double *x, size_t n, const double kernel[3], double *out);
#endif

#if RSD_HAVE_SSE41
extern size_t rsd_count_nonzero_sse41(const uint8_t *x, size_t n);
extern void rsd_convolve3_sse41(const double *x, size_t n, const double kernel[3], double *out);
#endif

#if RSD_HAVE_AVX2
extern size_t rsd_count_nonzero_avx2(const uint8_t *x, size_t n);
extern void rsd_convolve3_avx2(const double *x, size_t n, const double kernel[3], double *out);
#endif

#if RSD_HAVE_AVX512
extern size_t rsd_count_nonzero_avx512(const uint8_t *x, size_t n);
extern void rsd_convolve3_avx512(const double *x, size_t n, const double kernel[3], double *out);
#endif

#if RSD_HAVE_NEON
extern size_t rsd_count_nonzero_neon(const uint8_t *x, size_t n);
extern void rsd_convolve3_neon(const double *x, size_t n, const double kernel[3], double *out);
#endif

#if RSD_HAVE_WASM_SIMD128
extern size_t rsd_count_nonzero_wasm_simd128(const uint8_t *x, size_t n);
extern void rsd_convolve3_wasm_simd128(const double *x, size_t n, const double kernel[3], double *out);
#endif

static const RsdOps rsd_ops_scalar = {
    rsd_count_nonzero_scalar,
    rsd_convolve3_scalar
};

#if RSD_HAVE_SSE2
static const RsdOps rsd_ops_sse2 = {
    rsd_count_nonzero_sse2,
    rsd_convolve3_sse2
};
#endif

#if RSD_HAVE_SSE41
static const RsdOps rsd_ops_sse41 = {
    rsd_count_nonzero_sse41,
    rsd_convolve3_sse41
};
#endif

#if RSD_HAVE_AVX2
static const RsdOps rsd_ops_avx2 = {
    rsd_count_nonzero_avx2,
    rsd_convolve3_avx2
};
#endif

#if RSD_HAVE_AVX512
static const RsdOps rsd_ops_avx512 = {
    rsd_count_nonzero_avx512,
    rsd_convolve3_avx512
};
#endif

#if RSD_HAVE_NEON
static const RsdOps rsd_ops_neon = {
    rsd_count_nonzero_neon,
    rsd_convolve3_neon
};
#endif

#if RSD_HAVE_WASM_SIMD128
static const RsdOps rsd_ops_wasm_simd128 = {
    rsd_count_nonzero_wasm_simd128,
    rsd_convolve3_wasm_simd128
};
#endif

static const RsdOps *rsd_ops = &rsd_ops_scalar;
static int rsd_dispatch_initialized = 0;
static char rsd_requested_backend_buf[32] = "auto";
static char rsd_selected_backend_buf[32] = "uninitialized";

static const char *const rsd_backend_order[] = {
    "avx512",
    "avx2",
    "sse41",
    "sse2",
    "neon",
    "wasm_simd128",
    "scalar"
};

int rsd_backend_known(const char *backend) {
    if (backend == NULL) {
        return 0;
    }
    return strcmp(backend, "scalar") == 0 ||
           strcmp(backend, "sse2") == 0 ||
           strcmp(backend, "sse41") == 0 ||
           strcmp(backend, "avx2") == 0 ||
           strcmp(backend, "avx512") == 0 ||
           strcmp(backend, "neon") == 0 ||
           strcmp(backend, "wasm_simd128") == 0;
}

int rsd_backend_compiled(const char *backend) {
    if (backend == NULL) {
        return 0;
    }
    if (strcmp(backend, "scalar") == 0) {
        return 1;
    }
    if (strcmp(backend, "sse2") == 0) {
        return RSD_HAVE_SSE2 != 0;
    }
    if (strcmp(backend, "sse41") == 0) {
        return RSD_HAVE_SSE41 != 0;
    }
    if (strcmp(backend, "avx2") == 0) {
        return RSD_HAVE_AVX2 != 0;
    }
    if (strcmp(backend, "avx512") == 0) {
        return RSD_HAVE_AVX512 != 0;
    }
    if (strcmp(backend, "neon") == 0) {
        return RSD_HAVE_NEON != 0;
    }
    if (strcmp(backend, "wasm_simd128") == 0) {
        return RSD_HAVE_WASM_SIMD128 != 0;
    }
    return 0;
}

int rsd_backend_cpu_supported(const char *backend) {
    if (backend == NULL) {
        return 0;
    }
    if (strcmp(backend, "scalar") == 0) {
        return 1;
    }
    if (strcmp(backend, "sse2") == 0) {
        return rsd_cpu_has_sse2();
    }
    if (strcmp(backend, "sse41") == 0) {
        return rsd_cpu_has_sse41();
    }
    if (strcmp(backend, "avx2") == 0) {
        return rsd_cpu_has_avx2();
    }
    if (strcmp(backend, "avx512") == 0) {
        return rsd_cpu_has_avx512();
    }
    if (strcmp(backend, "neon") == 0) {
        return rsd_cpu_has_neon();
    }
    if (strcmp(backend, "wasm_simd128") == 0) {
        return rsd_cpu_has_wasm_simd128();
    }
    return 0;
}

int rsd_backend_available(const char *backend) {
    return rsd_backend_compiled(backend) && rsd_backend_cpu_supported(backend);
}

static const RsdOps *rsd_backend_ops(const char *backend) {
    if (strcmp(backend, "scalar") == 0) {
        return &rsd_ops_scalar;
    }
#if RSD_HAVE_SSE2
    if (strcmp(backend, "sse2") == 0) {
        return &rsd_ops_sse2;
    }
#endif
#if RSD_HAVE_SSE41
    if (strcmp(backend, "sse41") == 0) {
        return &rsd_ops_sse41;
    }
#endif
#if RSD_HAVE_AVX2
    if (strcmp(backend, "avx2") == 0) {
        return &rsd_ops_avx2;
    }
#endif
#if RSD_HAVE_AVX512
    if (strcmp(backend, "avx512") == 0) {
        return &rsd_ops_avx512;
    }
#endif
#if RSD_HAVE_NEON
    if (strcmp(backend, "neon") == 0) {
        return &rsd_ops_neon;
    }
#endif
#if RSD_HAVE_WASM_SIMD128
    if (strcmp(backend, "wasm_simd128") == 0) {
        return &rsd_ops_wasm_simd128;
    }
#endif
    return NULL;
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
    size_t n = sizeof(rsd_backend_order) / sizeof(rsd_backend_order[0]);
    for (size_t i = 0; i < n; ++i) {
        const char *candidate = rsd_backend_order[i];
        if (rsd_backend_available(candidate)) {
            return candidate;
        }
    }
    return "scalar";
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

void rsd_convolve3_valid(const double *x, size_t n, const double kernel[3], double *out) {
    if (!rsd_dispatch_initialized) {
        rsd_init_dispatch();
    }
    rsd_ops->convolve3(x, n, kernel, out);
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
