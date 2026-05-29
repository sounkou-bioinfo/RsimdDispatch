# Runtime Dispatch Semantics

`RsimdDispatch` uses a single R shared library. All compiled variants
are linked into that shared object, and an operation-table pointer
selects the active implementation set.

This means backend switching is safe in one R process:

``` r

library(RsimdDispatch)
x <- as.raw(c(0, 1, 2, 3))
a <- c(1, 2, 3)
b <- c(10, 100)

simd_set_backend("scalar")
count_nonzero(x)
#> [1] 3
convolve1d(a, b)
#> [1]  10 120 230 300

candidate <- setdiff(simd_info()$available_backends, "scalar")[1]
if (!is.na(candidate)) {
  simd_set_backend(candidate)
  count_nonzero(x)
  convolve1d(a, b)
}
#> [1]  10 120 230 300

simd_set_backend("auto")
simd_backend()
#> [1] "avx512"
```

[`simd_set_backend()`](https://sounkou-bioinfo.github.io/RsimdDispatch/reference/simd_set_backend.md)
performs two checks for explicit choices:

- the backend was compiled into this package build;
- the current CPU/runtime supports the backend, including OS support for
  AVX and AVX-512 register state on x86 and module-level SIMD128 support
  on WebAssembly.

There is intentionally no unsafe force mode. If a backend is not
available, the setter errors before any SIMD-only instruction can
execute. Backend selection is a process-global operation-table pointer:
initialize or change it from ordinary R code, and do not call
[`simd_set_backend()`](https://sounkou-bioinfo.github.io/RsimdDispatch/reference/simd_set_backend.md)
concurrently with active native worker threads that are executing
dispatched kernels.

``` r

simd_info()[c("compiled_backends", "cpu_supported_backends", "available_backends")]
#> $compiled_backends
#> [1] "scalar" "sse2"   "sse41"  "avx2"   "avx512"
#> 
#> $cpu_supported_backends
#> [1] "scalar" "sse2"   "sse41"  "avx2"   "avx512"
#> 
#> $available_backends
#> [1] "scalar" "sse2"   "sse41"  "avx2"   "avx512"
```

## SIMDe and native ISA compilation

The SIMD source files use SIMDe types and functions, but the backend
names refer to native target-specific staged kernel objects. During
configuration, each optional backend is accepted only if the compiler
flag and SIMDe header together define the expected native SIMDe macro.
For example, the AVX2 probe compiles with `-mavx2`, includes
`<simde/x86/avx2.h>`, and requires `SIMDE_X86_AVX2_NATIVE`. AVX-512
similarly requires the native F, BW, and VL macros. On Emscripten/webR,
the WebAssembly SIMD128 probe compiles with `-msimd128`, includes
`<simde/wasm/simd128.h>`, and requires `SIMDE_WASM_SIMD128_NATIVE`.

Those flags make the compiler define target macros such as `__AVX2__`,
`__AVX512BW__`, or `__wasm_simd128__`. SIMDe maps those to
`SIMDE_ARCH_*` and then to `SIMDE_*_NATIVE`; the SIMDe function bodies
use native intrinsics under those macros and fall back only when the
native macro is absent. The generated `src/Makevars` links the staged
objects into the package shared library while the dispatcher, CPU
feature detection, and R API are compiled by R’s ordinary `src/Makevars`
path.

The installed diagnostics report the backends that passed the
SIMDe-native compile probe:

``` r

simd_info()[c("compiled_backends", "simde_native_backends", "simde_version", "simde_commit")]
#> $compiled_backends
#> [1] "scalar" "sse2"   "sse41"  "avx2"   "avx512"
#> 
#> $simde_native_backends
#> [1] "sse2"   "sse41"  "avx2"   "avx512"
#> 
#> $simde_version
#> [1] "0.8.4"
#> 
#> $simde_commit
#> [1] "f3e8262173b7089db9a9d57a9ecef8dd07ad9c97"
```

## Benchmarking backend switching

The following benchmark is evaluated when this article is built. It uses
a small input so package checks remain fast, but it still exercises
same-process backend switching through the public API.

``` r

if (requireNamespace("bench", quietly = TRUE)) {
  bench_x <- rep(as.raw(c(0, 1, 2, 3, 0, 255, 7, 0)), length.out = 2^20)

  bench <- bench::mark(
    scalar = {
      simd_set_backend("scalar")
      count_nonzero(bench_x)
    },
    auto = {
      simd_set_backend("auto")
      count_nonzero(bench_x)
    },
    iterations = 5,
    check = TRUE
  )

  simd_set_backend("auto")
  bench[, c("expression", "median", "itr/sec", "n_itr")]
}
#> # A tibble: 2 × 3
#>   expression   median `itr/sec`
#>   <bch:expr> <bch:tm>     <dbl>
#> 1 scalar      367.3µs     2771.
#> 2 auto         26.8µs    31847.
```

The same switch applies to the full one-dimensional convolution demo:

``` r

if (requireNamespace("bench", quietly = TRUE)) {
  bench_a <- runif(10000)
  bench_b <- runif(100)

  conv_bench <- bench::mark(
    scalar = {
      simd_set_backend("scalar")
      convolve1d(bench_a, bench_b)
    },
    auto = {
      simd_set_backend("auto")
      convolve1d(bench_a, bench_b)
    },
    iterations = 5,
    check = TRUE
  )

  simd_set_backend("auto")
  conv_bench[, c("expression", "median", "itr/sec", "n_itr")]
}
#> # A tibble: 2 × 3
#>   expression   median `itr/sec`
#>   <bch:expr> <bch:tm>     <dbl>
#> 1 scalar        330µs     3028.
#> 2 auto          181µs     5530.
```

`"auto"` selects the best backend from the compiled and supported
intersection. The current ranking is:

``` text
avx512 > avx2 > sse41 > sse2 > neon > wasm_simd128 > scalar
```

Architecture guards mean x86 systems normally consider x86 backends, ARM
systems normally consider NEON, and webR/Emscripten builds can consider
WebAssembly SIMD128. Scalar is always available.
