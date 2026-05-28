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
#> [1] "avx512"
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
#> [1] "scalar" "sse2"   "sse41"  "avx2"   "avx512"
#> 
#> $available_backends
#> [1] "scalar" "sse2"   "sse41"  "avx2"   "avx512"
```

`"auto"` selects the best backend from the compiled and supported
intersection. The current ranking is:

``` text
avx512 > avx2 > sse41 > sse2 > neon > scalar
```

Architecture guards mean x86 systems normally consider x86 backends and
ARM systems normally consider NEON. Scalar is always available.
