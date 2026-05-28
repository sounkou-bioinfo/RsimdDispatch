#include <R.h>
#include <R_ext/Rdynload.h>
#include <Rinternals.h>

#include "simd_dispatch.h"

SEXP RC_count_nonzero(SEXP x);
SEXP RC_convolve3(SEXP x, SEXP kernel);
SEXP RC_simd_set_backend(SEXP backend_s);
SEXP RC_simd_backend(void);
SEXP RC_simd_info(void);

static const R_CallMethodDef call_methods[] = {
    {"RC_count_nonzero",     (DL_FUNC)&RC_count_nonzero,     1},
    {"RC_convolve3",         (DL_FUNC)&RC_convolve3,         2},
    {"RC_simd_set_backend",  (DL_FUNC)&RC_simd_set_backend,  1},
    {"RC_simd_backend",      (DL_FUNC)&RC_simd_backend,      0},
    {"RC_simd_info",         (DL_FUNC)&RC_simd_info,         0},
    {NULL, NULL, 0}
};

void R_init_RsimdDispatch(DllInfo *dll) {
    rsd_init_dispatch();
    R_registerRoutines(dll, NULL, call_methods, NULL, NULL);
    R_useDynamicSymbols(dll, FALSE);
}
