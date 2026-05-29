# Select the runtime SIMD backend

Select the backend used by subsequent calls to dispatched demo kernels
such as
[`count_nonzero()`](https://sounkou-bioinfo.github.io/RsimdDispatch/reference/count_nonzero.md)
and
[`convolve1d()`](https://sounkou-bioinfo.github.io/RsimdDispatch/reference/convolve1d.md).
`RsimdDispatch` keeps all compiled variants in one shared object and
switches guarded operation tables. This makes same-process benchmarking
possible.

## Usage

``` r
simd_set_backend(backend = "auto")
```

## Arguments

- backend:

  Character scalar. Use `"auto"` to select the best available backend,
  or one of `simd_info()$available_backends` for an explicit choice.

## Value

The selected backend, invisibly. For `"auto"`, this is the backend
chosen from the compiled and CPU-supported set.

## Examples

``` r
old <- simd_backend()
simd_set_backend("scalar")
simd_set_backend("auto")
```
