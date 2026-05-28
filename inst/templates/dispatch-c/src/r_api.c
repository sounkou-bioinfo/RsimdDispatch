#include <R.h>
#include <Rinternals.h>

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "config.h"
#include "cpu_features.h"
#include "simd_dispatch.h"

#ifndef RSD_SIMDE_VERSION
#define RSD_SIMDE_VERSION "unknown"
#endif
#ifndef RSD_SIMDE_COMMIT
#define RSD_SIMDE_COMMIT "unknown"
#endif
#ifndef RSD_SIMDE_NATIVE_SSE2
#define RSD_SIMDE_NATIVE_SSE2 0
#endif
#ifndef RSD_SIMDE_NATIVE_SSE41
#define RSD_SIMDE_NATIVE_SSE41 0
#endif
#ifndef RSD_SIMDE_NATIVE_AVX2
#define RSD_SIMDE_NATIVE_AVX2 0
#endif
#ifndef RSD_SIMDE_NATIVE_AVX512
#define RSD_SIMDE_NATIVE_AVX512 0
#endif
#ifndef RSD_SIMDE_NATIVE_NEON
#define RSD_SIMDE_NATIVE_NEON 0
#endif

static const char *rsd_string_scalar(SEXP x, const char *arg) {
    if (TYPEOF(x) != STRSXP || XLENGTH(x) != 1 || STRING_ELT(x, 0) == NA_STRING) {
        Rf_error("%s must be a non-missing character scalar", arg);
    }
    return CHAR(STRING_ELT(x, 0));
}

SEXP RC_count_nonzero(SEXP x) {
    if (TYPEOF(x) != RAWSXP) {
        Rf_error("x must be a raw vector");
    }
    R_xlen_t len = XLENGTH(x);
    if ((uint64_t)len > (uint64_t)SIZE_MAX) {
        Rf_error("x is too large for this platform");
    }
    size_t count = rsd_count_nonzero((const uint8_t *)RAW(x), (size_t)len);
    return Rf_ScalarReal((double)count);
}

SEXP RC_simd_set_backend(SEXP backend_s) {
    const char *backend = rsd_string_scalar(backend_s, "backend");
    rsd_set_backend(backend);
    return Rf_mkString(rsd_selected_backend());
}

SEXP RC_simd_backend(void) {
    return Rf_mkString(rsd_selected_backend());
}

/* Returns a protected STRSXP; caller must UNPROTECT. */
static int rsd_backend_simde_native(const char *backend) {
    if (strcmp(backend, "sse2") == 0) {
        return RSD_SIMDE_NATIVE_SSE2 != 0;
    }
    if (strcmp(backend, "sse41") == 0) {
        return RSD_SIMDE_NATIVE_SSE41 != 0;
    }
    if (strcmp(backend, "avx2") == 0) {
        return RSD_SIMDE_NATIVE_AVX2 != 0;
    }
    if (strcmp(backend, "avx512") == 0) {
        return RSD_SIMDE_NATIVE_AVX512 != 0;
    }
    if (strcmp(backend, "neon") == 0) {
        return RSD_SIMDE_NATIVE_NEON != 0;
    }
    return 0;
}

static SEXP rsd_backend_character_vector(int mode) {
    const char *backends[] = {"scalar", "sse2", "sse41", "avx2", "avx512", "neon"};
    const int n_backends = (int)(sizeof(backends) / sizeof(backends[0]));
    int include[6] = {0, 0, 0, 0, 0, 0};
    int n = 0;

    for (int i = 0; i < n_backends; ++i) {
        if (mode == 0) {
            include[i] = rsd_backend_compiled(backends[i]);
        } else if (mode == 1) {
            include[i] = rsd_backend_cpu_supported(backends[i]);
        } else if (mode == 2) {
            include[i] = rsd_backend_available(backends[i]);
        } else {
            include[i] = rsd_backend_simde_native(backends[i]);
        }
        n += include[i] != 0;
    }

    SEXP out = PROTECT(Rf_allocVector(STRSXP, n));
    int j = 0;
    for (int i = 0; i < n_backends; ++i) {
        if (include[i]) {
            SET_STRING_ELT(out, j, Rf_mkChar(backends[i]));
            ++j;
        }
    }
    return out;
}

SEXP RC_simd_info(void) {
    const char *names[] = {
        "dispatch_mode",
        "requested_backend",
        "selected_backend",
        "compiled_backends",
        "cpu_supported_backends",
        "available_backends",
        "simde_native_backends",
        "cpu_sse2",
        "cpu_sse41",
        "cpu_avx2",
        "cpu_avx512",
        "cpu_neon",
        "target_arch",
        "target_os",
        "simde_version",
        "simde_commit"
    };
    const int n = (int)(sizeof(names) / sizeof(names[0]));
    SEXP out = PROTECT(Rf_allocVector(VECSXP, n));
    SEXP out_names = PROTECT(Rf_allocVector(STRSXP, n));

    /* Force lazy auto-selection so selected_backend is always meaningful. */
    rsd_init_dispatch();

    SET_VECTOR_ELT(out, 0, Rf_mkString(rsd_dispatch_mode()));
    SET_VECTOR_ELT(out, 1, Rf_mkString(rsd_requested_backend()));
    SET_VECTOR_ELT(out, 2, Rf_mkString(rsd_selected_backend()));
    SET_VECTOR_ELT(out, 3, rsd_backend_character_vector(0)); UNPROTECT(1);
    SET_VECTOR_ELT(out, 4, rsd_backend_character_vector(1)); UNPROTECT(1);
    SET_VECTOR_ELT(out, 5, rsd_backend_character_vector(2)); UNPROTECT(1);
    SET_VECTOR_ELT(out, 6, rsd_backend_character_vector(3)); UNPROTECT(1);
    SET_VECTOR_ELT(out, 7, Rf_ScalarLogical(rsd_cpu_has_sse2() != 0));
    SET_VECTOR_ELT(out, 8, Rf_ScalarLogical(rsd_cpu_has_sse41() != 0));
    SET_VECTOR_ELT(out, 9, Rf_ScalarLogical(rsd_cpu_has_avx2() != 0));
    SET_VECTOR_ELT(out, 10, Rf_ScalarLogical(rsd_cpu_has_avx512() != 0));
    SET_VECTOR_ELT(out, 11, Rf_ScalarLogical(rsd_cpu_has_neon() != 0));
    SET_VECTOR_ELT(out, 12, Rf_mkString(rsd_target_arch()));
    SET_VECTOR_ELT(out, 13, Rf_mkString(rsd_target_os()));
    SET_VECTOR_ELT(out, 14, Rf_mkString(RSD_SIMDE_VERSION));
    SET_VECTOR_ELT(out, 15, Rf_mkString(RSD_SIMDE_COMMIT));

    for (int i = 0; i < n; ++i) {
        SET_STRING_ELT(out_names, i, Rf_mkChar(names[i]));
    }
    Rf_setAttrib(out, R_NamesSymbol, out_names);
    UNPROTECT(2);
    return out;
}
