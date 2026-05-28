# Getting Started with RsimdDispatch

`RsimdDispatch` demonstrates a single-shared-library runtime dispatch
pattern for C code in R packages. The package stages scalar and SIMD
kernel objects during configuration and switches among compiled,
CPU-supported implementations through a guarded operation table.

``` r

library(RsimdDispatch)

x <- as.raw(c(0, 1, 2, 0, 255))
y <- c(1, 2, 4, 8, 16)
count_nonzero(x)
#> [1] 3
convolve3(y, c(0.25, 0.5, 0.25))
#> [1] 2.25 4.50 9.00
simd_backend()
#> [1] "avx2"
```

Inspect the installed build:

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

Switching is allowed in the same R process:

``` r

simd_set_backend("scalar")
count_nonzero(x)
#> [1] 3
convolve3(y, c(1, 0, -1))
#> [1]  -3  -6 -12

candidate <- setdiff(simd_info()$available_backends, "scalar")[1]
if (!is.na(candidate)) {
  simd_set_backend(candidate)
  count_nonzero(x)
  convolve3(y, c(1, 0, -1))
}
#> [1]  -3  -6 -12

simd_set_backend("auto")
simd_backend()
#> [1] "avx2"
```

An explicit backend is accepted only when it was compiled and the
current CPU/runtime supports it. Unsupported choices error before any
ISA-specific code runs.
