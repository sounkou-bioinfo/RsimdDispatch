
# simdDispatch

A small, self-contained C library for runtime SIMD dispatch. The build
system probes the compiler for each ISA at configure time, compiles
matching kernel objects with the appropriate flags, and links them into
a static archive. At runtime, CPU feature detection selects the best
available backend; the caller invokes an operation through a single
`sd_dispatch_invoke()` call and the library routes it to the right
kernel.

`simd_dispatch.c`, `cpu_features.c`, and the scalar kernel have no
external dependencies beyond a C11 compiler. SIMDe is a dependency of
the SIMD backend kernels only (`kernels/kernel_sse2.c`, `kernel_avx2.c`,
etc.) — the dispatch core and the scalar path are completely independent
of it. If SIMDe headers are absent the build falls back to a scalar-only
library.

## Building

``` bash
cd tools/simdDispatch
make          # builds build/libsimd_dispatch.a
make test     # builds and runs the test suite
make examples # builds example binaries
make info     # shows what the compiler supports on this machine
```

`SIMDE_DIR` can be overridden if SIMDe headers are not at
`../../inst/include`:

``` bash
make SIMDE_DIR=/path/to/simde-parent test
```

## What the compiler sees on this machine

``` bash
cd "$(git rev-parse --show-toplevel)/tools/simdDispatch" && make info 2>/dev/null
```

    SIMDe found : 1  (../../inst/include)
    SSE2        : 1
    SSE4.1      : 1
    AVX2        : 1
    AVX-512     : 1
    NEON        : 0
    WASM SIMD   : 0

## Test results

``` bash
cd "$(git rev-parse --show-toplevel)/tools/simdDispatch" && make test 2>/dev/null
```

    ./build/test_dispatch
    simdDispatch C tests
    --------------------
      PASS  target arch/os strings are non-empty
      PASS  sd_init_dispatch is idempotent
      PASS  selected_backend is not 'uninitialized' after init
      PASS  auto backend selects a real backend
      PASS  sd_backend_count > 0
      PASS  all backend names are non-NULL and non-empty
      PASS  sd_backend_name out-of-bounds returns NULL
      PASS  scalar backend is known
      PASS  scalar backend is compiled
      PASS  scalar backend is always available
      PASS  unknown backend is not known
      PASS  requested_backend returns 'auto' after sd_set_backend(auto)
      PASS  sd_set_backend(scalar) selects scalar
      PASS  custom error handler called for unknown backend
      PASS  count_nonzero(empty) == 0
      PASS  count_nonzero(all zeros) == 0
      PASS  count_nonzero(all ones, n=64) == 64
      PASS  count_nonzero mixed buffer == 5
      PASS  count_nonzero large buffer with known pattern
      PASS  convolve1d with identity kernel [1]
      PASS  convolve1d [1,2,3] * [1,0,-1] = [1,2,2,-2,-3]
      PASS  convolve1d na=0 does not write output
      PASS  count_nonzero == 3 on every available backend
      PASS  convolve1d result matches on every available backend
    --------------------
    All tests passed.

## API

Initialise the library, optionally select a backend, fill a call-frame
struct, and invoke:

``` c_run
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include "simd_dispatch.h"
#include "cpu_features.h"
#include "kernels/kernel_api.h"

int main(void) {
    sd_init_dispatch();
    printf("arch    : %s\n", sd_target_arch());
    printf("os      : %s\n", sd_target_os());
    printf("backend : %s\n", sd_selected_backend());

    const uint8_t buf[] = {0, 1, 2, 0, 3, 0, 0, 4, 5, 0};
    SdCountNonzeroCall call = { .x = buf, .n = sizeof(buf), .result = 0 };
    sd_dispatch_invoke(SD_OP_COUNT_NONZERO, SD_SIG_RAW_COUNT, &call, "count_nonzero");
    printf("count_nonzero = %zu  (expected 5)\n", call.result);

    const double a[] = {1.0, 2.0, 3.0}, b[] = {1.0, 0.0, -1.0};
    double out[5] = {0};
    SdConvolve1dCall conv = { .a = a, .na = 3, .b = b, .nb = 3, .out = out };
    sd_dispatch_invoke(SD_OP_CONVOLVE1D, SD_SIG_F64_CONVOLVE, &conv, "convolve1d");
    printf("convolve1d    = [%.0f, %.0f, %.0f, %.0f, %.0f]  (expected [1, 2, 2, -2, -3])\n",
           out[0], out[1], out[2], out[3], out[4]);
    return 0;
}
```

    arch    : x86_64
    os      : linux
    backend : avx2
    count_nonzero = 5  (expected 5)
    convolve1d    = [1, 2, 2, -2, -3]  (expected [1, 2, 2, -2, -3])

`sd_dispatch_invoke` writes results into the call-frame struct; there is
no return value and no allocation. `sd_init_dispatch` is idempotent.

The error handler is pluggable. The default writes to stderr and calls
`abort()`; the R adapter installs one that redirects to `Rf_error()`.

## Operations

The two operations compiled into the reference kernels are
`SD_OP_COUNT_NONZERO` (`uint8_t` buffer → `size_t`) and
`SD_OP_CONVOLVE1D` (`double` arrays → `double` output). Adding an
operation requires entries in `kernels/kernel_api.h` (operation enum
value, signature enum value, call-frame struct), a kernel function per
backend, and a `SdKernelDef` table registered with
`sd_register_kernel_table()`.

## Benchmarks

Wall-clock timings comparing the scalar backend against the best
available backend for each operation. Each measurement runs the
operation 2000 times and reports the median nanoseconds per call.

``` c_run
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <time.h>
#include "simd_dispatch.h"
#include "kernels/kernel_api.h"

#define N_RUNS   2000
#define BUF_N    (1 << 20)   /* 1 M bytes  */
#define VEC_N    (1 << 14)   /* 16 K doubles */
#define KERN_N   64

static double median_ns(struct timespec *times, int n) {
    /* simple insertion sort then pick middle */
    for (int i = 1; i < n; ++i) {
        struct timespec key = times[i]; int j = i - 1;
        while (j >= 0 && (times[j].tv_sec > key.tv_sec ||
               (times[j].tv_sec == key.tv_sec && times[j].tv_nsec > key.tv_nsec))) {
            times[j+1] = times[j]; --j;
        }
        times[j+1] = key;
    }
    long s = times[n/2].tv_sec, ns = times[n/2].tv_nsec;
    return (double)s * 1e9 + (double)ns;
}

static void bench_op(const char *backend, const char *op_name,
                     SdOperation op, SdKernelSignature sig, void *call,
                     struct timespec *scratch) {
    sd_set_backend(backend);
    for (int i = 0; i < N_RUNS; ++i) {
        struct timespec t0, t1;
        clock_gettime(CLOCK_MONOTONIC, &t0);
        sd_dispatch_invoke(op, sig, call, op_name);
        clock_gettime(CLOCK_MONOTONIC, &t1);
        scratch[i].tv_sec  = t1.tv_sec  - t0.tv_sec;
        scratch[i].tv_nsec = t1.tv_nsec - t0.tv_nsec;
        if (scratch[i].tv_nsec < 0) { scratch[i].tv_sec--; scratch[i].tv_nsec += 1000000000L; }
    }
}

int main(void) {
    sd_init_dispatch();

    /* find best available backend (last in list that is available) */
    const char *best = "scalar";
    for (size_t i = 0; i < sd_backend_count(); ++i) {
        const char *name = sd_backend_name(i);
        if (sd_backend_available(name)) best = name;
    }
    printf("scalar vs %s  (%d runs each)\n\n", best, N_RUNS);

    static uint8_t buf[BUF_N];
    memset(buf, 0x5a, sizeof(buf));
    SdCountNonzeroCall cnt = { .x = buf, .n = BUF_N, .result = 0 };

    static double a[VEC_N], b[KERN_N], out[VEC_N + KERN_N - 1];
    for (size_t i = 0; i < VEC_N;  ++i) a[i]   = (double)i / VEC_N;
    for (size_t i = 0; i < KERN_N; ++i) b[i]   = 1.0 / KERN_N;
    SdConvolve1dCall conv = { .a = a, .na = VEC_N, .b = b, .nb = KERN_N, .out = out };

    static struct timespec scratch[N_RUNS];

    bench_op("scalar", "count_nonzero", SD_OP_COUNT_NONZERO, SD_SIG_RAW_COUNT, &cnt, scratch);
    double cnt_scalar = median_ns(scratch, N_RUNS);
    bench_op(best, "count_nonzero", SD_OP_COUNT_NONZERO, SD_SIG_RAW_COUNT, &cnt, scratch);
    double cnt_best = median_ns(scratch, N_RUNS);

    printf("count_nonzero  (n=%d)\n", BUF_N);
    printf("  scalar  %8.0f ns\n", cnt_scalar);
    printf("  %-7s %8.0f ns  (%.1fx)\n\n", best, cnt_best, cnt_scalar / cnt_best);

    bench_op("scalar", "convolve1d", SD_OP_CONVOLVE1D, SD_SIG_F64_CONVOLVE, &conv, scratch);
    double conv_scalar = median_ns(scratch, N_RUNS);
    bench_op(best, "convolve1d", SD_OP_CONVOLVE1D, SD_SIG_F64_CONVOLVE, &conv, scratch);
    double conv_best = median_ns(scratch, N_RUNS);

    printf("convolve1d     (na=%d, nb=%d)\n", VEC_N, KERN_N);
    printf("  scalar  %8.0f ns\n", conv_scalar);
    printf("  %-7s %8.0f ns  (%.1fx)\n", best, conv_best, conv_scalar / conv_best);

    sd_set_backend("auto");
    return 0;
}
```

    scalar vs avx2  (2000 runs each)

    count_nonzero  (n=1048576)
      scalar    233724 ns
      avx2       15212 ns  (15.4x)

    convolve1d     (na=16384, nb=64)
      scalar    246936 ns
      avx2      131821 ns  (1.9x)
