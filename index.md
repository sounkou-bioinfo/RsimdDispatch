# RsimdDispatch

Pure-C runtime SIMD dispatch templates for R packages. Stage scalar,
SSE, AVX, AVX-512, NEON, and WebAssembly SIMD128 kernel objects during
configuration; link one shared library and switch among compiled and
CPU-supported implementations at runtime without unsafe overrides.

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

## Use in other packages

Copy the C dispatch scaffold from your package root:

``` r

RsimdDispatch::use_simd_dispatch(pkg = "MyPackage", prefix = "mypkg")
```

The helper updates `DESCRIPTION` with `LinkingTo: RsimdDispatch`, writes
the scaffold files, and substitutes package-specific registration and C
symbol prefixes. It does not add a runtime dependency on
`RsimdDispatch`. Replace the demo
[`count_nonzero()`](https://sounkou-bioinfo.github.io/RsimdDispatch/reference/count_nonzero.md)
and
[`convolve3()`](https://sounkou-bioinfo.github.io/RsimdDispatch/reference/convolve3.md)
kernels under `tools/kernels/` with your own
scalar/SSE/AVX/NEON/WebAssembly kernels. The generated `configure`
script probes compiler support, stages selected kernel objects in
`src/rsd-kernels/`, and leaves `src/Makevars` to link those objects with
the baseline R API, CPU detection, and dispatcher.

See the package vignettes for the downstream-package workflow and
dispatch semantics.

## API

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
simd_set_backend("scalar")
simd_backend()
#> [1] "scalar"
simd_set_backend("avx2")
simd_backend()
#> [1] "avx2"
simd_set_backend("auto")
simd_info()[c("compiled_backends", "cpu_supported_backends", "available_backends", "simde_native_backends")]
#> $compiled_backends
#> [1] "scalar" "sse2"   "sse41"  "avx2"   "avx512"
#> 
#> $cpu_supported_backends
#> [1] "scalar" "sse2"   "sse41"  "avx2"  
#> 
#> $available_backends
#> [1] "scalar" "sse2"   "sse41"  "avx2"  
#> 
#> $simde_native_backends
#> [1] "sse2"   "sse41"  "avx2"   "avx512"
simde_info()[c("version", "commit")]
#> $version
#> [1] "0.8.4"
#> 
#> $commit
#> [1] "f3e8262173b7089db9a9d57a9ecef8dd07ad9c97"
simd_dispatch_template_path()
#> [1] "/usr/local/lib/R/site-library/RsimdDispatch/templates/dispatch-c"
```

[`simd_set_backend()`](https://sounkou-bioinfo.github.io/RsimdDispatch/reference/simd_set_backend.md)
rejects uncompiled or CPU-unsupported backends. There is no unsafe
override.

## Example

``` r

count_nonzero(x)
#> [1] 3
convolve3(y, c(1, 0, -1))
#> [1]  -3  -6 -12
simd_info()[c("selected_backend", "available_backends")]
#> $selected_backend
#> [1] "avx2"
#> 
#> $available_backends
#> [1] "scalar" "sse2"   "sse41"  "avx2"
```

Switching backends is same-process because all variants are linked into
one R shared library and selected through a guarded operation table.

``` r

simd_set_backend("scalar")
scalar_count <- count_nonzero(x)
scalar_conv <- convolve3(y, c(1, 0, -1))

simd_set_backend("avx2")
avx2_count <- count_nonzero(x)
avx2_conv <- convolve3(y, c(1, 0, -1))

simd_set_backend("auto")
data.frame(
  scalar = scalar_count,
  avx2 = avx2_count,
  convolution_equal = identical(scalar_conv, avx2_conv),
  selected_after_auto = simd_backend()
)
#>   scalar avx2 convolution_equal selected_after_auto
#> 1      3    3              TRUE                avx2
```

## Scalar vs AVX2 benchmarks

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
| scalar  |    11.395 |      4594.631 |         20 |             1.000 |
| avx2    |     2.005 |     25790.055 |         20 |             5.613 |

The same runtime switch can benchmark a numeric three-tap
convolution/FIR kernel.

``` r

set.seed(2)
y <- runif(2 * 1024 * 1024)
k <- c(0.25, 0.5, 0.25)

conv_mark <- bench::mark(
  scalar = {
    simd_set_backend("scalar")
    convolve3(y, k)
  },
  avx2 = {
    simd_set_backend("avx2")
    convolve3(y, k)
  },
  iterations = 10,
  check = TRUE,
  memory = FALSE
)

simd_set_backend("auto")

conv_bench <- data.frame(
  backend = as.character(conv_mark$expression),
  median_ms = as.numeric(conv_mark$median) * 1000,
  million_outputs_per_second = (length(y) - 2) * as.numeric(conv_mark$`itr/sec`) / 1e6,
  iterations = conv_mark$n_itr,
  row.names = NULL
)
conv_bench$speedup_vs_scalar <- conv_bench$million_outputs_per_second /
  conv_bench$million_outputs_per_second[conv_bench$backend == "scalar"]

knitr::kable(conv_bench, digits = 3)
```

| backend | median_ms | million_outputs_per_second | iterations | speedup_vs_scalar |
|:--------|----------:|---------------------------:|-----------:|------------------:|
| scalar  |     4.453 |                    473.137 |          9 |             1.000 |
| avx2    |     4.408 |                    475.333 |          9 |             1.005 |

## Development

### Updating bundled SIMDe

This is a maintainer workflow for `RsimdDispatch` itself, not something
downstream packages need to run. Downstream packages use
`LinkingTo: RsimdDispatch` and the copied dispatch template; they do not
re-vendor SIMDe.

`RsimdDispatch` vendors the full SIMDe header tree under
`inst/include/simde`. To update the pinned checkout and regenerate
bundled-code notices:

``` sh
Rscript tools/vendor-simde.R
Rscript tools/update-authors.R
```

Provenance lives in `inst/vendor/simde/VERSION`, `inst/AUTHORS`, and
`inst/LICENCE.note`.

### Package maintenance

The top-level Makefile uses GNU make.

``` sh
make rd       # roxygen docs and NAMESPACE
make readme   # evaluate README.Rmd and write README.md
make test
make check
bash tools/webr-build-check.sh  # build and check the webR/WebAssembly package
```
