#include <R.h>
#include <Rinternals.h>

#include <stdint.h>
#include <stddef.h>

#include "config.h"
#include <simdDispatch/cpu_features.h>
#include <simdDispatch/simd_dispatch.h>
#include <simdDispatch/kernels/kernel_api.h>

static const char *sd_string_scalar(SEXP x, const char *arg) {
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
    SdCountNonzeroCall call = {
        .x = (const uint8_t *)RAW(x),
        .n = (size_t)len,
        .result = 0
    };
    sd_dispatch_invoke(SD_OP_COUNT_NONZERO, SD_SIG_RAW_COUNT, &call, "count_nonzero");
    return Rf_ScalarReal((double)call.result);
}

SEXP RC_convolve1d(SEXP a, SEXP b) {
    if (TYPEOF(a) != REALSXP) {
        Rf_error("a must be a numeric vector");
    }
    if (TYPEOF(b) != REALSXP) {
        Rf_error("b must be a numeric vector");
    }
    R_xlen_t na = XLENGTH(a);
    R_xlen_t nb = XLENGTH(b);
    if ((uint64_t)na > (uint64_t)SIZE_MAX || (uint64_t)nb > (uint64_t)SIZE_MAX) {
        Rf_error("input is too large for this platform");
    }
    R_xlen_t nab = 0;
    if (na > 0 && nb > 0) {
        if (na > R_XLEN_T_MAX - nb + 1) {
            Rf_error("convolution output is too large");
        }
        nab = na + nb - 1;
        if ((uint64_t)nab > (uint64_t)SIZE_MAX) {
            Rf_error("convolution output is too large for this platform");
        }
    }
    SEXP out = PROTECT(Rf_allocVector(REALSXP, nab));
    if (nab > 0) {
        SdConvolve1dCall call = {
            .a = REAL(a),
            .na = (size_t)na,
            .b = REAL(b),
            .nb = (size_t)nb,
            .out = REAL(out)
        };
        sd_dispatch_invoke(SD_OP_CONVOLVE1D, SD_SIG_F64_CONVOLVE, &call, "convolve1d");
    }
    UNPROTECT(1);
    return out;
}

SEXP RC_simd_set_backend(SEXP backend_s) {
    const char *backend = sd_string_scalar(backend_s, "backend");
    sd_set_backend(backend);
    return Rf_mkString(sd_selected_backend());
}

SEXP RC_simd_backend(void) {
    return Rf_mkString(sd_selected_backend());
}

/* Which membership predicate to apply when building a backend character
 * vector.  This replaces the former magic integer argument to
 * sd_backend_included(). */
typedef enum {
    SD_BACKEND_FILTER_COMPILED      = 0,
    SD_BACKEND_FILTER_CPU_SUPPORTED = 1,
    SD_BACKEND_FILTER_AVAILABLE     = 2,
    SD_BACKEND_FILTER_SIMDE_NATIVE  = 3
} SdBackendFilter;

static int sd_backend_included(const char *backend, SdBackendFilter filter) {
    switch (filter) {
        case SD_BACKEND_FILTER_COMPILED:      return sd_backend_compiled(backend);
        case SD_BACKEND_FILTER_CPU_SUPPORTED: return sd_backend_cpu_supported(backend);
        case SD_BACKEND_FILTER_AVAILABLE:     return sd_backend_available(backend);
        case SD_BACKEND_FILTER_SIMDE_NATIVE:  return sd_backend_simde_native(backend);
        default:                               return 0;
    }
}

static SEXP sd_backend_character_vector(SdBackendFilter filter) {
    size_t n_backends = sd_backend_count();
    int n = 0;

    for (size_t i = 0; i < n_backends; ++i) {
        const char *backend = sd_backend_name(i);
        n += sd_backend_included(backend, filter) != 0;
    }

    SEXP out = PROTECT(Rf_allocVector(STRSXP, n));
    int j = 0;
    for (size_t i = 0; i < n_backends; ++i) {
        const char *backend = sd_backend_name(i);
        if (sd_backend_included(backend, filter)) {
            SET_STRING_ELT(out, j, Rf_mkChar(backend));
            ++j;
        }
    }
    return out;
}

static SEXP sd_operation_character_vector(void) {
    size_t n_operations = sd_operation_count();
    SEXP out = PROTECT(Rf_allocVector(STRSXP, (R_xlen_t)n_operations));
    for (size_t i = 0; i < n_operations; ++i) {
        SET_STRING_ELT(out, (R_xlen_t)i, Rf_mkChar(sd_operation_name((SdOperation)i)));
    }
    return out;
}

static SEXP sd_operation_backend_vector(SdOperation operation) {
    size_t n_backends = sd_backend_count();
    int n = 0;

    for (size_t i = 0; i < n_backends; ++i) {
        const char *backend = sd_backend_name(i);
        n += sd_backend_operation_available(backend, operation) != 0;
    }

    SEXP out = PROTECT(Rf_allocVector(STRSXP, n));
    int j = 0;
    for (size_t i = 0; i < n_backends; ++i) {
        const char *backend = sd_backend_name(i);
        if (sd_backend_operation_available(backend, operation)) {
            SET_STRING_ELT(out, j, Rf_mkChar(backend));
            ++j;
        }
    }
    return out;
}

static SEXP sd_operation_backends_list(void) {
    size_t n_operations = sd_operation_count();
    SEXP out = PROTECT(Rf_allocVector(VECSXP, (R_xlen_t)n_operations));
    SEXP out_names = PROTECT(Rf_allocVector(STRSXP, (R_xlen_t)n_operations));

    for (size_t i = 0; i < n_operations; ++i) {
        SdOperation op = (SdOperation)i;
        SET_STRING_ELT(out_names, (R_xlen_t)i, Rf_mkChar(sd_operation_name(op)));
        SET_VECTOR_ELT(out, (R_xlen_t)i, sd_operation_backend_vector(op));
        UNPROTECT(1);
    }
    Rf_setAttrib(out, R_NamesSymbol, out_names);
    UNPROTECT(1);
    return out;
}

static SEXP sd_operation_selected_backends_vector(void) {
    size_t n_operations = sd_operation_count();
    SEXP out = PROTECT(Rf_allocVector(STRSXP, (R_xlen_t)n_operations));
    SEXP out_names = PROTECT(Rf_allocVector(STRSXP, (R_xlen_t)n_operations));

    for (size_t i = 0; i < n_operations; ++i) {
        SdOperation op = (SdOperation)i;
        const char *backend = sd_operation_selected_backend(op);
        SET_STRING_ELT(out_names, (R_xlen_t)i, Rf_mkChar(sd_operation_name(op)));
        SET_STRING_ELT(out, (R_xlen_t)i, backend != NULL ? Rf_mkChar(backend) : NA_STRING);
    }
    Rf_setAttrib(out, R_NamesSymbol, out_names);
    UNPROTECT(1);
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
        "operations",
        "operation_backends",
        "operation_selected_backends",
        "cpu_sse2",
        "cpu_sse41",
        "cpu_avx2",
        "cpu_avx512",
        "cpu_neon",
        "cpu_wasm_simd128",
        "target_arch",
        "target_os"
    };
    const int n = (int)(sizeof(names) / sizeof(names[0]));
    SEXP out = PROTECT(Rf_allocVector(VECSXP, n));
    SEXP out_names = PROTECT(Rf_allocVector(STRSXP, n));

    /* Force lazy auto-selection so selected_backend is always meaningful. */
    sd_init_dispatch();

    SET_VECTOR_ELT(out, 0, Rf_mkString(sd_dispatch_mode()));
    SET_VECTOR_ELT(out, 1, Rf_mkString(sd_requested_backend()));
    SET_VECTOR_ELT(out, 2, Rf_mkString(sd_selected_backend()));
    SET_VECTOR_ELT(out, 3, sd_backend_character_vector(SD_BACKEND_FILTER_COMPILED));      UNPROTECT(1);
    SET_VECTOR_ELT(out, 4, sd_backend_character_vector(SD_BACKEND_FILTER_CPU_SUPPORTED)); UNPROTECT(1);
    SET_VECTOR_ELT(out, 5, sd_backend_character_vector(SD_BACKEND_FILTER_AVAILABLE));     UNPROTECT(1);
    SET_VECTOR_ELT(out, 6, sd_backend_character_vector(SD_BACKEND_FILTER_SIMDE_NATIVE));  UNPROTECT(1);
    SET_VECTOR_ELT(out, 7, sd_operation_character_vector()); UNPROTECT(1);
    SET_VECTOR_ELT(out, 8, sd_operation_backends_list()); UNPROTECT(1);
    SET_VECTOR_ELT(out, 9, sd_operation_selected_backends_vector()); UNPROTECT(1);
    SET_VECTOR_ELT(out, 10, Rf_ScalarLogical(sd_cpu_has_sse2() != 0));
    SET_VECTOR_ELT(out, 11, Rf_ScalarLogical(sd_cpu_has_sse41() != 0));
    SET_VECTOR_ELT(out, 12, Rf_ScalarLogical(sd_cpu_has_avx2() != 0));
    SET_VECTOR_ELT(out, 13, Rf_ScalarLogical(sd_cpu_has_avx512() != 0));
    SET_VECTOR_ELT(out, 14, Rf_ScalarLogical(sd_cpu_has_neon() != 0));
    SET_VECTOR_ELT(out, 15, Rf_ScalarLogical(sd_cpu_has_wasm_simd128() != 0));
    SET_VECTOR_ELT(out, 16, Rf_mkString(sd_target_arch()));
    SET_VECTOR_ELT(out, 17, Rf_mkString(sd_target_os()));

    for (int i = 0; i < n; ++i) {
        SET_STRING_ELT(out_names, i, Rf_mkChar(names[i]));
    }
    Rf_setAttrib(out, R_NamesSymbol, out_names);
    UNPROTECT(2);
    return out;
}
