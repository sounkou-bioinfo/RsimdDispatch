# Runtime Dispatch Semantics

`RsimdDispatch` uses a single R shared library. All compiled variants
are linked into that shared object, and the active dispatch table stores
the resolved backend for each operation slot.

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

candidate <- setdiff(simd_info()$operation_backends$convolve1d, "scalar")[1]
if (!is.na(candidate)) {
  simd_set_backend(candidate)
  count_nonzero(x)
  convolve1d(a, b)
}
#> [1]  10 120 230 300

simd_set_backend("auto")
simd_backend()
#> [1] "avx2"
```

[`simd_set_backend()`](https://sounkou-bioinfo.github.io/RsimdDispatch/reference/simd_set_backend.md)
performs two checks for explicit choices:

- the backend was compiled into this package build;
- the current CPU/runtime supports the backend, including OS support for
  AVX and AVX-512 register state on x86 and module-level SIMD128 support
  on WebAssembly.

If a backend is not available, the setter errors before any SIMD-only
instruction can execute. Backend files register only the operation slots
they implement, so a backend may deliberately omit an operation.
Explicitly selecting such a backend is allowed, but calling an
unsupported operation errors clearly. With `"auto"`, each operation
resolves independently to the best available backend that registered
that operation. The summary `selected_backend` is a single backend when
all resolved slots agree and `"mixed"` when different operations resolve
to different backends.

Backend selection is process-global: initialize or change it from
ordinary R code, and do not call
[`simd_set_backend()`](https://sounkou-bioinfo.github.io/RsimdDispatch/reference/simd_set_backend.md)
concurrently with active native worker threads that are executing
dispatched kernels.

``` r

simd_info()[c("compiled_backends", "cpu_supported_backends", "available_backends", "operation_backends", "operation_selected_backends")]
#> $compiled_backends
#> [1] "scalar" "sse2"   "sse41"  "avx2"   "avx512"
#> 
#> $cpu_supported_backends
#> [1] "scalar" "sse2"   "sse41"  "avx2"  
#> 
#> $available_backends
#> [1] "scalar" "sse2"   "sse41"  "avx2"  
#> 
#> $operation_backends
#> $operation_backends$count_nonzero
#> [1] "scalar" "sse2"   "sse41"  "avx2"  
#> 
#> $operation_backends$convolve1d
#> [1] "scalar" "avx2"  
#> 
#> 
#> $operation_selected_backends
#> count_nonzero    convolve1d 
#>        "avx2"        "avx2"
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

simd_info()[c("compiled_backends", "simde_native_backends")]
#> $compiled_backends
#> [1] "scalar" "sse2"   "sse41"  "avx2"   "avx512"
#> 
#> $simde_native_backends
#> [1] "sse2"   "sse41"  "avx2"   "avx512"
```

SIMDe provenance (version, commit, date) is available separately via
[`simde_info()`](https://sounkou-bioinfo.github.io/RsimdDispatch/reference/simde_info.md),
which reads from `inst/vendor/simde/VERSION`.

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
#> 1 scalar      330.2µs     2947.
#> 2 auto         36.3µs    22240.
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
#> 1 scalar        446µs     2194.
#> 2 auto          188µs     5201.
```

`"auto"` selects the best backend from the compiled and supported
intersection that also supports the operation being called. The current
ranking is:

``` text
avx512 > avx2 > sse41 > sse2 > neon > wasm_simd128 > scalar
```

Architecture guards mean x86 systems normally consider x86 backends, ARM
systems normally consider NEON, and webR/Emscripten builds can consider
WebAssembly SIMD128. Scalar is always available.
