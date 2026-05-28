# Runtime Dispatch Semantics

`RsimdDispatch` uses a single R shared library. All compiled variants
are linked into that shared object, and a function pointer selects the
active implementation.

This means backend switching is safe in one R process:

``` r

library(RsimdDispatch)
x <- as.raw(c(0, 1, 2, 3))

simd_set_backend("scalar")
count_nonzero(x)
#> [1] 3

candidate <- setdiff(simd_info()$available_backends, "scalar")[1]
if (!is.na(candidate)) {
  simd_set_backend(candidate)
  count_nonzero(x)
}
#> [1] 3

simd_set_backend("auto")
simd_backend()
#> [1] "avx2"
```

[`simd_set_backend()`](https://sounkou-bioinfo.github.io/RsimdDispatch/reference/simd_set_backend.md)
performs two checks for explicit choices:

- the backend was compiled into this package build;
- the current CPU/runtime supports the backend, including OS support for
  AVX and AVX-512 register state on x86.

There is intentionally no unsafe force mode. If a backend is not
available, the setter errors before any SIMD-only instruction can
execute.

``` r

simd_info()[c("compiled_backends", "cpu_supported_backends", "available_backends")]
#> $compiled_backends
#> [1] "scalar" "sse2"   "sse41"  "avx2"   "avx512"
#> 
#> $cpu_supported_backends
#> [1] "scalar" "sse2"   "sse41"  "avx2"  
#> 
#> $available_backends
#> [1] "scalar" "sse2"   "sse41"  "avx2"
```

## SIMDe and native ISA compilation

The SIMD source files use SIMDe types and functions, but the backend
names refer to native target-specific translation units. During
configuration, each optional backend is accepted only if the compiler
flag and SIMDe header together define the expected native SIMDe macro.
For example, the AVX2 probe compiles with `-mavx2`, includes
`<simde/x86/avx2.h>`, and requires `SIMDE_X86_AVX2_NATIVE`. AVX-512
similarly requires the native F, BW, and VL macros.

Those flags make the compiler define target macros such as `__AVX2__` or
`__AVX512BW__`. SIMDe maps those to `SIMDE_ARCH_*` and then to
`SIMDE_*_NATIVE`; the SIMDe function bodies use native intrinsics under
those macros and fall back only when the native macro is absent. The
dispatcher, CPU feature detection, and R API are still compiled without
those ISA flags.

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
#> 1 scalar      476.6µs     2103.
#> 2 auto         53.8µs    17326.
```

`"auto"` selects the best backend from the compiled and supported
intersection. The current ranking is:

``` text
avx512 > avx2 > sse41 > sse2 > neon > scalar
```

Architecture guards mean x86 systems normally consider x86 backends and
ARM systems normally consider NEON. Scalar is always available.
