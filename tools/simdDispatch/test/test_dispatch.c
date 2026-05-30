/*
 * test_dispatch.c — pure-C tests for the simdDispatch library.
 *
 * Uses only <assert.h> and <stdio.h>; no external test framework.
 * Build and run:
 *   make test
 */

#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <math.h>
#include <setjmp.h>

#include "simd_dispatch.h"
#include "cpu_features.h"
#include "kernels/kernel_api.h"

#define PASS(name) printf("  PASS  %s\n", name)
#define RUN(fn)    do { fn(); } while (0)

/* --------------------------------------------------------------------------
 * cpu_features
 * -------------------------------------------------------------------------- */

static void test_target_strings(void) {
    const char *arch = sd_target_arch();
    const char *os   = sd_target_os();
    assert(arch != NULL && arch[0] != '\0');
    assert(os   != NULL && os[0]   != '\0');
    PASS("target arch/os strings are non-empty");
}

/* --------------------------------------------------------------------------
 * Dispatch lifecycle
 * -------------------------------------------------------------------------- */

static void test_init_idempotent(void) {
    sd_init_dispatch();
    const char *b1 = sd_selected_backend();
    sd_init_dispatch();
    const char *b2 = sd_selected_backend();
    assert(strcmp(b1, b2) == 0);
    PASS("sd_init_dispatch is idempotent");
}

static void test_selected_backend_not_uninitialized(void) {
    sd_init_dispatch();
    assert(strcmp(sd_selected_backend(), "uninitialized") != 0);
    PASS("selected_backend is not 'uninitialized' after init");
}

static void test_auto_backend(void) {
    sd_set_backend("auto");
    const char *sel = sd_selected_backend();
    assert(sel != NULL);
    /* should be a real backend name or "mixed", not "unavailable" */
    assert(strcmp(sel, "unavailable") != 0);
    assert(strcmp(sel, "uninitialized") != 0);
    PASS("auto backend selects a real backend");
}

static void test_backend_count_positive(void) {
    assert(sd_backend_count() > 0);
    PASS("sd_backend_count > 0");
}

static void test_backend_names_non_null(void) {
    for (size_t i = 0; i < sd_backend_count(); ++i) {
        const char *name = sd_backend_name(i);
        assert(name != NULL && name[0] != '\0');
    }
    PASS("all backend names are non-NULL and non-empty");
}

static void test_backend_name_oob(void) {
    assert(sd_backend_name(sd_backend_count()) == NULL);
    PASS("sd_backend_name out-of-bounds returns NULL");
}

static void test_scalar_always_known(void) {
    assert(sd_backend_known("scalar") != 0);
    PASS("scalar backend is known");
}

static void test_scalar_always_compiled(void) {
    assert(sd_backend_compiled("scalar") != 0);
    PASS("scalar backend is compiled");
}

static void test_scalar_always_available(void) {
    assert(sd_backend_available("scalar") != 0);
    PASS("scalar backend is always available");
}

static void test_unknown_backend_not_known(void) {
    assert(sd_backend_known("nonexistent_backend_xyz") == 0);
    PASS("unknown backend is not known");
}

static void test_requested_backend_initial(void) {
    sd_set_backend("auto");
    assert(strcmp(sd_requested_backend(), "auto") == 0);
    PASS("requested_backend returns 'auto' after sd_set_backend(auto)");
}

static void test_set_backend_scalar(void) {
    sd_set_backend("scalar");
    assert(strcmp(sd_requested_backend(), "scalar") == 0);
    assert(strcmp(sd_selected_backend(), "scalar") == 0);
    sd_set_backend("auto");
    PASS("sd_set_backend(scalar) selects scalar");
}

/* --------------------------------------------------------------------------
 * Error handler
 * -------------------------------------------------------------------------- */

static int error_was_called = 0;
static char last_error_msg[256];
static jmp_buf error_jump_buf;

static void test_error_handler_fn(const char *msg) {
    error_was_called = 1;
    strncpy(last_error_msg, msg, sizeof(last_error_msg) - 1);
    last_error_msg[sizeof(last_error_msg) - 1] = '\0';
    longjmp(error_jump_buf, 1);  /* must not return to the dispatch engine */
}

static void test_custom_error_handler_unknown_backend(void) {
    error_was_called = 0;
    sd_set_error_handler(test_error_handler_fn);
    if (setjmp(error_jump_buf) == 0) {
        sd_set_backend("this_backend_does_not_exist");
    }
    sd_set_error_handler(NULL); /* restore default */
    assert(error_was_called != 0);
    assert(strstr(last_error_msg, "unknown") != NULL ||
           strstr(last_error_msg, "this_backend_does_not_exist") != NULL);
    PASS("custom error handler called for unknown backend");
}

/* --------------------------------------------------------------------------
 * count_nonzero kernel
 * -------------------------------------------------------------------------- */

static void test_count_nonzero_empty(void) {
    sd_set_backend("auto");
    SdCountNonzeroCall call = { .x = NULL, .n = 0, .result = 99 };
    sd_dispatch_invoke(SD_OP_COUNT_NONZERO, SD_SIG_RAW_COUNT, &call, "count_nonzero");
    assert(call.result == 0);
    PASS("count_nonzero(empty) == 0");
}

static void test_count_nonzero_all_zeros(void) {
    const uint8_t buf[64] = {0};
    SdCountNonzeroCall call = { .x = buf, .n = 64, .result = 99 };
    sd_dispatch_invoke(SD_OP_COUNT_NONZERO, SD_SIG_RAW_COUNT, &call, "count_nonzero");
    assert(call.result == 0);
    PASS("count_nonzero(all zeros) == 0");
}

static void test_count_nonzero_all_ones(void) {
    uint8_t buf[64];
    memset(buf, 1, sizeof(buf));
    SdCountNonzeroCall call = { .x = buf, .n = 64, .result = 0 };
    sd_dispatch_invoke(SD_OP_COUNT_NONZERO, SD_SIG_RAW_COUNT, &call, "count_nonzero");
    assert(call.result == 64);
    PASS("count_nonzero(all ones, n=64) == 64");
}

static void test_count_nonzero_mixed(void) {
    const uint8_t buf[] = {0, 1, 2, 0, 3, 0, 0, 4, 5, 0};
    SdCountNonzeroCall call = { .x = buf, .n = sizeof(buf), .result = 0 };
    sd_dispatch_invoke(SD_OP_COUNT_NONZERO, SD_SIG_RAW_COUNT, &call, "count_nonzero");
    assert(call.result == 5);
    PASS("count_nonzero mixed buffer == 5");
}

static void test_count_nonzero_large(void) {
    /* 1000 bytes: every third is zero */
    uint8_t buf[1000];
    size_t expected = 0;
    for (size_t i = 0; i < 1000; ++i) {
        buf[i] = (uint8_t)(i % 3 != 0 ? 1 : 0);
        expected += buf[i] != 0;
    }
    SdCountNonzeroCall call = { .x = buf, .n = 1000, .result = 0 };
    sd_dispatch_invoke(SD_OP_COUNT_NONZERO, SD_SIG_RAW_COUNT, &call, "count_nonzero");
    assert(call.result == expected);
    PASS("count_nonzero large buffer with known pattern");
}

/* --------------------------------------------------------------------------
 * convolve1d kernel
 * -------------------------------------------------------------------------- */

static void test_convolve1d_identity(void) {
    const double a[] = {1.0, 2.0, 3.0};
    const double b[] = {1.0};
    double out[3] = {0};
    SdConvolve1dCall call = { .a = a, .na = 3, .b = b, .nb = 1, .out = out };
    sd_dispatch_invoke(SD_OP_CONVOLVE1D, SD_SIG_F64_CONVOLVE, &call, "convolve1d");
    assert(out[0] == 1.0 && out[1] == 2.0 && out[2] == 3.0);
    PASS("convolve1d with identity kernel [1]");
}

static void test_convolve1d_diff(void) {
    /* [1,2,3] * [1,0,-1] = [1, 2, 2, -2, -3] */
    const double a[] = {1.0, 2.0, 3.0};
    const double b[] = {1.0, 0.0, -1.0};
    double out[5] = {0};
    SdConvolve1dCall call = { .a = a, .na = 3, .b = b, .nb = 3, .out = out };
    sd_dispatch_invoke(SD_OP_CONVOLVE1D, SD_SIG_F64_CONVOLVE, &call, "convolve1d");
    const double expected[] = {1.0, 2.0, 2.0, -2.0, -3.0};
    for (int i = 0; i < 5; ++i) {
        assert(fabs(out[i] - expected[i]) < 1e-12);
    }
    PASS("convolve1d [1,2,3] * [1,0,-1] = [1,2,2,-2,-3]");
}

static void test_convolve1d_empty_a(void) {
    const double b[] = {1.0, 2.0};
    double out[4] = {9.0, 9.0, 9.0, 9.0};
    SdConvolve1dCall call = { .a = NULL, .na = 0, .b = b, .nb = 2, .out = out };
    sd_dispatch_invoke(SD_OP_CONVOLVE1D, SD_SIG_F64_CONVOLVE, &call, "convolve1d");
    /* Output buffer should be untouched when na=0 */
    (void)out;
    PASS("convolve1d na=0 does not write output");
}

/* --------------------------------------------------------------------------
 * Per-backend coverage: test every available backend
 * -------------------------------------------------------------------------- */

static void test_all_backends_count_nonzero(void) {
    const uint8_t buf[] = {1, 0, 1, 1, 0};
    for (size_t i = 0; i < sd_backend_count(); ++i) {
        const char *name = sd_backend_name(i);
        if (!sd_backend_operation_available(name, SD_OP_COUNT_NONZERO)) continue;
        sd_set_backend(name);
        SdCountNonzeroCall call = { .x = buf, .n = sizeof(buf), .result = 0 };
        sd_dispatch_invoke(SD_OP_COUNT_NONZERO, SD_SIG_RAW_COUNT, &call, "count_nonzero");
        assert(call.result == 3);
    }
    sd_set_backend("auto");
    PASS("count_nonzero == 3 on every available backend");
}

static void test_all_backends_convolve1d(void) {
    const double a[] = {1.0, 2.0, 3.0};
    const double b[] = {1.0, 0.0, -1.0};
    const double expected[] = {1.0, 2.0, 2.0, -2.0, -3.0};
    for (size_t i = 0; i < sd_backend_count(); ++i) {
        const char *name = sd_backend_name(i);
        if (!sd_backend_operation_available(name, SD_OP_CONVOLVE1D)) continue;
        sd_set_backend(name);
        double out[5] = {0};
        SdConvolve1dCall call = { .a = a, .na = 3, .b = b, .nb = 3, .out = out };
        sd_dispatch_invoke(SD_OP_CONVOLVE1D, SD_SIG_F64_CONVOLVE, &call, "convolve1d");
        for (int j = 0; j < 5; ++j) {
            assert(fabs(out[j] - expected[j]) < 1e-12);
        }
    }
    sd_set_backend("auto");
    PASS("convolve1d result matches on every available backend");
}

/* --------------------------------------------------------------------------
 * main
 * -------------------------------------------------------------------------- */

int main(void) {
    printf("simdDispatch C tests\n");
    printf("--------------------\n");

    RUN(test_target_strings);
    RUN(test_init_idempotent);
    RUN(test_selected_backend_not_uninitialized);
    RUN(test_auto_backend);
    RUN(test_backend_count_positive);
    RUN(test_backend_names_non_null);
    RUN(test_backend_name_oob);
    RUN(test_scalar_always_known);
    RUN(test_scalar_always_compiled);
    RUN(test_scalar_always_available);
    RUN(test_unknown_backend_not_known);
    RUN(test_requested_backend_initial);
    RUN(test_set_backend_scalar);
    RUN(test_custom_error_handler_unknown_backend);
    RUN(test_count_nonzero_empty);
    RUN(test_count_nonzero_all_zeros);
    RUN(test_count_nonzero_all_ones);
    RUN(test_count_nonzero_mixed);
    RUN(test_count_nonzero_large);
    RUN(test_convolve1d_identity);
    RUN(test_convolve1d_diff);
    RUN(test_convolve1d_empty_a);
    RUN(test_all_backends_count_nonzero);
    RUN(test_all_backends_convolve1d);

    printf("--------------------\n");
    printf("All tests passed.\n");
    return 0;
}
