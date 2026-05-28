#include "config.h"
#include "simd_dispatch.h"
#include "cpu_features.h"

#include <R.h>

#include <stdio.h>
#include <string.h>

extern size_t rsd_count_nonzero_scalar(const uint8_t *x, size_t n);

#if RSD_HAVE_SSE2
extern size_t rsd_count_nonzero_sse2(const uint8_t *x, size_t n);
#endif

#if RSD_HAVE_SSE41
extern size_t rsd_count_nonzero_sse41(const uint8_t *x, size_t n);
#endif

#if RSD_HAVE_AVX2
extern size_t rsd_count_nonzero_avx2(const uint8_t *x, size_t n);
#endif

#if RSD_HAVE_AVX512
extern size_t rsd_count_nonzero_avx512(const uint8_t *x, size_t n);
#endif

#if RSD_HAVE_NEON
extern size_t rsd_count_nonzero_neon(const uint8_t *x, size_t n);
#endif

static rsd_count_nonzero_fn rsd_count_nonzero_impl = rsd_count_nonzero_scalar;
static int rsd_dispatch_initialized = 0;
static char rsd_requested_backend_buf[32] = "auto";
static char rsd_selected_backend_buf[32] = "uninitialized";

static const char *const rsd_backend_order[] = {
    "avx512",
    "avx2",
    "sse41",
    "sse2",
    "neon",
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
           strcmp(backend, "neon") == 0;
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
    return 0;
}

int rsd_backend_available(const char *backend) {
    return rsd_backend_compiled(backend) && rsd_backend_cpu_supported(backend);
}

static rsd_count_nonzero_fn rsd_backend_function(const char *backend) {
    if (strcmp(backend, "scalar") == 0) {
        return rsd_count_nonzero_scalar;
    }
#if RSD_HAVE_SSE2
    if (strcmp(backend, "sse2") == 0) {
        return rsd_count_nonzero_sse2;
    }
#endif
#if RSD_HAVE_SSE41
    if (strcmp(backend, "sse41") == 0) {
        return rsd_count_nonzero_sse41;
    }
#endif
#if RSD_HAVE_AVX2
    if (strcmp(backend, "avx2") == 0) {
        return rsd_count_nonzero_avx2;
    }
#endif
#if RSD_HAVE_AVX512
    if (strcmp(backend, "avx512") == 0) {
        return rsd_count_nonzero_avx512;
    }
#endif
#if RSD_HAVE_NEON
    if (strcmp(backend, "neon") == 0) {
        return rsd_count_nonzero_neon;
    }
#endif
    return NULL;
}

static void rsd_select_backend_unchecked(const char *backend) {
    rsd_count_nonzero_fn fn = rsd_backend_function(backend);
    if (fn == NULL) {
        Rf_error("internal error: no function pointer for backend '%s'", backend);
    }
    rsd_count_nonzero_impl = fn;
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
    return rsd_count_nonzero_impl(x, n);
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

static const char *rsd_backends_csv(int want_compiled, int want_supported, int want_available) {
    static char bufs[3][128];
    static int next = 0;
    char *out = bufs[next];
    next = (next + 1) % 3;
    out[0] = '\0';

    const char *names[] = {"scalar", "sse2", "sse41", "avx2", "avx512", "neon"};
    size_t n = sizeof(names) / sizeof(names[0]);
    for (size_t i = 0; i < n; ++i) {
        const char *name = names[i];
        int include = 0;
        if (want_available) {
            include = rsd_backend_available(name);
        } else if (want_compiled) {
            include = rsd_backend_compiled(name);
        } else if (want_supported) {
            include = rsd_backend_cpu_supported(name);
        }
        if (include) {
            if (out[0] != '\0') {
                strncat(out, ",", 127 - strlen(out));
            }
            strncat(out, name, 127 - strlen(out));
        }
    }
    if (out[0] == '\0') {
        snprintf(out, 128, "%s", "none");
    }
    return out;
}

const char *rsd_compiled_backends_csv(void) {
    return rsd_backends_csv(1, 0, 0);
}

const char *rsd_cpu_supported_backends_csv(void) {
    return rsd_backends_csv(0, 1, 0);
}

const char *rsd_available_backends_csv(void) {
    return rsd_backends_csv(0, 0, 1);
}

const char *rsd_dispatch_mode(void) {
    return "single-shlib-function-pointer";
}
