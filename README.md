RsimdDispatch
================

# RsimdDispatch

`RsimdDispatch` is a pure-C demo/template package for runtime SIMD
multiversioning in R packages. It compiles scalar and ISA-specific C
translation units separately, then switches a guarded function pointer
at runtime:

``` r
simd_set_backend("scalar")
bench_scalar <- count_nonzero(x)

simd_set_backend("avx2")
bench_avx2 <- count_nonzero(x)

simd_set_backend("auto")
```

Unsupported or uncompiled backends are rejected before any ISA-specific
code can run.

## Build

From a checkout:

``` sh
R CMD INSTALL .
```

The package `configure` script checks compiler support for per-file
flags such as `-mavx2` and `-mavx512f -mavx512bw -mavx512vl`, writes
`src/config.h`, and generates `src/Makevars`. The R-facing API, CPU
feature detection, and dispatcher are compiled without global SIMD
flags.

Development helpers require GNU make:

``` sh
make rd       # regenerate roxygen docs and NAMESPACE
make readme   # evaluate README.Rmd and write README.md
make test
make check
```

## Demo

``` r
library(RsimdDispatch)

x <- as.raw(c(0, 1, 2, 0, 255))
count_nonzero(x)
#> [1] 3
simd_info()
#> $dispatch_mode
#> [1] "single-shlib-function-pointer"
#> 
#> $requested_backend
#> [1] "auto"
#> 
#> $selected_backend
#> [1] "avx2"
#> 
#> $compiled_backends
#> [1] "scalar,sse2,sse41,avx2,avx512"
#> 
#> $cpu_supported_backends
#> [1] "scalar,sse2,sse41,avx2"
#> 
#> $available_backends
#> [1] "scalar,sse2,sse41,avx2"
#> 
#> $cpu_sse2
#> [1] TRUE
#> 
#> $cpu_sse41
#> [1] TRUE
#> 
#> $cpu_avx2
#> [1] TRUE
#> 
#> $cpu_avx512
#> [1] FALSE
#> 
#> $cpu_neon
#> [1] FALSE
#> 
#> $target_arch
#> [1] "x86_64"
#> 
#> $target_os
#> [1] "linux"
#> 
#> $simde_commit
#> [1] "f3e8262173b7089db9a9d57a9ecef8dd07ad9c97"
```

## Same-process backend switching

`RsimdDispatch` keeps all compiled variants in one shared library and
switches function pointers. This differs from loader-based designs that
fix one backend shared library for the process lifetime.

``` r
simd_set_backend("scalar")
count_nonzero(x)
#> [1] 3

info <- simd_info()
if (grepl("(^|,)avx2(,|$)", info$available_backends)) {
  simd_set_backend("avx2")
  count_nonzero(x)
}
#> [1] 3

simd_set_backend("auto")
simd_backend()
#> [1] "avx2"
```

## Scalar vs AVX2 benchmark

This README evaluates the benchmark when AVX2 is both compiled and
supported on the current CPU. Otherwise the chunk reports why it skipped
AVX2.

``` r
set.seed(1)
n <- 50 * 1024 * 1024
x <- as.raw(sample.int(256L, n, replace = TRUE) - 1L)
reps <- 20L

bench_backend <- function(backend, x, reps) {
  simd_set_backend(backend)
  invisible(count_nonzero(x))
  invisible(gc())
  elapsed <- system.time({
    ans <- 0
    for (i in seq_len(reps)) {
      ans <- ans + count_nonzero(x)
    }
  })[["elapsed"]]
  data.frame(
    backend = simd_backend(),
    bytes = length(x),
    reps = reps,
    scanned_mb = length(x) * reps / 1e6,
    elapsed_seconds = elapsed,
    mb_per_second = length(x) * reps / 1e6 / elapsed,
    checksum = ans,
    row.names = NULL
  )
}

info <- simd_info()
bench <- bench_backend("scalar", x, reps)
if (grepl("(^|,)avx2(,|$)", info$available_backends)) {
  bench <- rbind(bench, bench_backend("avx2", x, reps))
  bench$speedup_vs_scalar <- bench$elapsed_seconds[bench$backend == "scalar"] / bench$elapsed_seconds
} else {
  message("AVX2 not available in this build/runtime; available backends: ", info$available_backends)
  bench$speedup_vs_scalar <- 1
}
simd_set_backend("auto")
bench
#>   backend    bytes reps scanned_mb elapsed_seconds mb_per_second   checksum
#> 1  scalar 52428800   20   1048.576           0.230      4559.026 1044488560
#> 2    avx2 52428800   20   1048.576           0.044     23831.273 1044488560
#>   speedup_vs_scalar
#> 1          1.000000
#> 2          5.227273
```

## Vendored SIMDe

The package vendors the full header-only SIMDe include tree in
`inst/include/simde`. Downstream packages can add:

``` text
LinkingTo: RsimdDispatch
```

and include SIMDe headers from C code:

``` c
#include <simde/x86/avx2.h>
#include <simde/arm/neon.h>
```

Vendoring is reproducible:

``` sh
Rscript tools/vendor-simde.R
Rscript tools/update-authors.R
```

The pinned upstream commit and license notes are recorded in
`inst/vendor/simde/VERSION`, `inst/AUTHORS`, and `inst/LICENCE.note`.
