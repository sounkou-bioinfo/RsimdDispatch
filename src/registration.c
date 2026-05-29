#include <R.h>
#include <R_ext/Rdynload.h>
#include <Rinternals.h>

#include "simd_dispatch.h"

#define RSD_CALL_METHODS(X) \
    X(RC_count_nonzero,    (SEXP x),          1) \
    X(RC_convolve1d,       (SEXP a, SEXP b),  2) \
    X(RC_simd_set_backend, (SEXP backend_s),  1) \
    X(RC_simd_backend,     (void),            0) \
    X(RC_simd_info,        (void),            0)

#define RSD_DECLARE_CALL(name, args, n_args) SEXP name args;
RSD_CALL_METHODS(RSD_DECLARE_CALL)
#undef RSD_DECLARE_CALL

#define RSD_REGISTER_CALL(name, args, n_args) {#name, (DL_FUNC)&name, n_args},
static const R_CallMethodDef call_methods[] = {
    RSD_CALL_METHODS(RSD_REGISTER_CALL)
    {NULL, NULL, 0}
};
#undef RSD_REGISTER_CALL

void R_init_RsimdDispatch(DllInfo *dll) {
    rsd_init_dispatch();
    R_registerRoutines(dll, NULL, call_methods, NULL, NULL);
    R_useDynamicSymbols(dll, FALSE);
}
