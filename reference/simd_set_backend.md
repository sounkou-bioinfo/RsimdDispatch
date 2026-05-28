# Select the runtime SIMD backend

Select the backend used by subsequent calls to
[`count_nonzero()`](https://sounkou-bioinfo.github.io/RsimdDispatch/reference/count_nonzero.md).
`RsimdDispatch` keeps all compiled variants in one shared object and
switches guarded function pointers. This makes same-process benchmarking
possible.

## Usage

``` r
simd_set_backend(
  backend = c("auto", "scalar", "sse2", "sse41", "avx2", "avx512", "neon")
)
```

## Arguments

- backend:

  One of `"auto"`, `"scalar"`, `"sse2"`, `"sse41"`, `"avx2"`,
  `"avx512"`, or `"neon"`.

## Value

The selected backend, invisibly. For `"auto"`, this is the backend
chosen from the compiled and CPU-supported set.

## Examples

``` r
old <- simd_backend()
simd_set_backend("scalar")
simd_set_backend("auto")
```
