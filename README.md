
# RsimdDispatch

<!-- badges: start -->

[![R-CMD-check](https://github.com/sounkou-bioinfo/RsimdDispatch/actions/workflows/R-CMD-check.yaml/badge.svg)](https://github.com/sounkou-bioinfo/RsimdDispatch/actions/workflows/R-CMD-check.yaml)
[![R-universe](https://sounkou-bioinfo.r-universe.dev/badges/RsimdDispatch)](https://sounkou-bioinfo.r-universe.dev/RsimdDispatch)
<!-- badges: end -->

Pure-C runtime SIMD dispatch templates for R packages. Compile scalar,
SSE, AVX, AVX-512, and NEON C translation units separately; switch among
the compiled and CPU-supported implementations at runtime without unsafe
overrides.

## Install

``` r
install.packages(
  "RsimdDispatch",
  repos = c("https://sounkou-bioinfo.r-universe.dev", "https://cloud.r-project.org")
)
```

From GitHub:

``` r
# install.packages("remotes")
remotes::install_github("sounkou-bioinfo/RsimdDispatch")
```

## API

``` r
library(RsimdDispatch)

x <- as.raw(c(0, 1, 2, 0, 255))

count_nonzero(x)
#> [1] 3
simd_backend()
#> [1] "avx2"
simd_set_backend("scalar")
simd_backend()
#> [1] "scalar"
simd_set_backend("avx2")
simd_backend()
#> [1] "avx2"
simd_set_backend("auto")
simd_info()[c("compiled_backends", "cpu_supported_backends", "available_backends")]
#> $compiled_backends
#> [1] "scalar" "sse2"   "sse41"  "avx2"   "avx512"
#> 
#> $cpu_supported_backends
#> [1] "scalar" "sse2"   "sse41"  "avx2"  
#> 
#> $available_backends
#> [1] "scalar" "sse2"   "sse41"  "avx2"
simd_dispatch_template_path()
#> [1] "/usr/local/lib/R/site-library/RsimdDispatch/templates/dispatch-c"
```

`simd_set_backend()` rejects uncompiled or CPU-unsupported backends.
There is no unsafe override.

## Example

``` r
count_nonzero(x)
#> [1] 3
simd_info()[c("selected_backend", "available_backends")]
#> $selected_backend
#> [1] "avx2"
#> 
#> $available_backends
#> [1] "scalar" "sse2"   "sse41"  "avx2"
```

Switching backends is same-process because all variants are linked into
one R shared library and selected through guarded function pointers.

``` r
simd_set_backend("scalar")
scalar_count <- count_nonzero(x)

simd_set_backend("avx2")
avx2_count <- count_nonzero(x)

simd_set_backend("auto")
data.frame(
  scalar = scalar_count,
  avx2 = avx2_count,
  selected_after_auto = simd_backend()
)
#>   scalar avx2 selected_after_auto
#> 1      3    3                avx2
```

## Scalar vs AVX2 benchmark

``` r
set.seed(1)
x <- as.raw(sample.int(256L, 50 * 1024 * 1024, replace = TRUE) - 1L)

mark <- bench::mark(
  scalar = {
    simd_set_backend("scalar")
    count_nonzero(x)
  },
  avx2 = {
    simd_set_backend("avx2")
    count_nonzero(x)
  },
  iterations = 20,
  check = TRUE,
  memory = FALSE
)

simd_set_backend("auto")

bench <- data.frame(
  backend = as.character(mark$expression),
  median_ms = as.numeric(mark$median) * 1000,
  mb_per_second = length(x) * as.numeric(mark$`itr/sec`) / 1e6,
  iterations = mark$n_itr,
  row.names = NULL
)
bench$speedup_vs_scalar <- bench$mb_per_second / bench$mb_per_second[bench$backend == "scalar"]

knitr::kable(bench, digits = 3)
```

| backend | median_ms | mb_per_second | iterations | speedup_vs_scalar |
|:--------|----------:|--------------:|-----------:|------------------:|
| scalar  |    11.251 |      4653.029 |         20 |             1.000 |
| avx2    |     2.146 |     24305.763 |         20 |             5.224 |

## Vendoring flow

SIMDe is vendored as full header tree under `inst/include/simde`.
Downstream packages can use:

``` text
LinkingTo: RsimdDispatch
```

and include headers such as:

``` c
#include <simde/x86/avx2.h>
#include <simde/arm/neon.h>
```

Update the vendored checkout and generated notices with:

``` sh
Rscript tools/vendor-simde.R
Rscript tools/update-authors.R
```

Provenance lives in `inst/vendor/simde/VERSION`, `inst/AUTHORS`, and
`inst/LICENCE.note`.

## Development

The top-level Makefile uses GNU make.

``` sh
make rd       # roxygen docs and NAMESPACE
make readme   # evaluate README.Rmd and write README.md
make test
make check
```
