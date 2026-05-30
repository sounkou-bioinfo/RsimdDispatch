
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

The library is initialised once and then used through three calls:

``` c
sd_init_dispatch();   /* auto-select the best compiled + available backend */
sd_set_backend("avx2");   /* or pick explicitly; "auto" reverts */

SdCountNonzeroCall call = { .x = buf, .n = n, .result = 0 };
sd_dispatch_invoke(SD_OP_COUNT_NONZERO, SD_SIG_RAW_COUNT, &call, "count_nonzero");
```

`sd_dispatch_invoke` writes the result into the call-frame struct; there
is no return value and no allocation. `sd_init_dispatch` is idempotent
and thread-safe for concurrent reads after the first call.

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

## Runtime diagnostics (via the R package)

``` r
info <- simd_info()
info[c("compiled_backends", "simde_native_backends",
       "available_backends", "target_arch", "target_os")]
```

    $compiled_backends
    [1] "scalar" "sse2"   "sse41"  "avx2"   "avx512"

    $simde_native_backends
    [1] "sse2"   "sse41"  "avx2"   "avx512"

    $available_backends
    [1] "scalar" "sse2"   "sse41"  "avx2"  

    $target_arch
    [1] "x86_64"

    $target_os
    [1] "linux"

## Benchmarks

Timings on this machine comparing scalar against the best available
backend for the two built-in operations.

``` r
if (requireNamespace("bench", quietly = TRUE)) {
  x <- rep(as.raw(c(0L, 1L, 2L, 3L, 0L, 255L, 7L, 0L)), length.out = 2^20)
  best <- tail(simd_info()$available_backends, 1)

  b1 <- bench::mark(
    scalar = { simd_set_backend("scalar"); count_nonzero(x) },
    best   = { simd_set_backend(best);   count_nonzero(x) },
    iterations = 20, check = TRUE
  )
  simd_set_backend("auto")
  print(b1[, c("expression", "median", "itr/sec")])

  a <- runif(2^14); kernel <- runif(64)
  b2 <- bench::mark(
    scalar = { simd_set_backend("scalar"); convolve1d(a, kernel) },
    best   = { simd_set_backend(best);   convolve1d(a, kernel) },
    iterations = 20, check = TRUE, relative = FALSE
  )
  simd_set_backend("auto")
  print(b2[, c("expression", "median", "itr/sec")])
} else {
  message("install the 'bench' package to see timings")
}
```

    # A tibble: 2 × 3
      expression   median `itr/sec`
      <bch:expr> <bch:tm>     <dbl>
    1 scalar      235.5µs     4230.
    2 best         18.7µs    46656.
    # A tibble: 2 × 3
      expression   median `itr/sec`
      <bch:expr> <bch:tm>     <dbl>
    1 scalar        468µs     2232.
    2 best          162µs     5873.
