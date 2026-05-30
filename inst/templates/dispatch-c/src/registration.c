#include <R.h>
#include <R_ext/Rdynload.h>
#include <Rinternals.h>

#include "../tools/simdDispatch/simd_dispatch.h"

/* Forward-declare all RC_* entry points (old-style, no prototype). */
#define RC_CALL(name, nargs) SEXP RC_##name();
#include "r_calls.def"
#undef RC_CALL

static const R_CallMethodDef call_methods[] = {
#define RC_CALL(name, nargs) {"RC_" #name, (DL_FUNC)&RC_##name, nargs},
#include "r_calls.def"
#undef RC_CALL
    {NULL, NULL, 0}
};

/* Error handler that redirects dispatch engine errors into R's error
 * mechanism. Installed before sd_init_dispatch() so any error raised
 * during eager initialisation is handled correctly. */
static void sd_r_error_handler(const char *msg) {
    Rf_error("%s", msg);
}

void R_init_RsimdDispatch(DllInfo *dll) {
    sd_set_error_handler(sd_r_error_handler);
    sd_init_dispatch();
    R_registerRoutines(dll, NULL, call_methods, NULL, NULL);
    R_useDynamicSymbols(dll, FALSE);
}
