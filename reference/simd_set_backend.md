# Select the runtime SIMD backend

Select the backend used by subsequent calls to dispatched demo kernels
such as
[`count_nonzero()`](https://sounkou-bioinfo.github.io/RsimdDispatch/reference/count_nonzero.md)
and
[`convolve3()`](https://sounkou-bioinfo.github.io/RsimdDispatch/reference/convolve3.md).
`RsimdDispatch` keeps all compiled variants in one shared object and
switches guarded operation tables. This makes same-process benchmarking
possible.

## Usage

``` r
simd_set_backend(
  backend = c("auto", "scalar", "sse2", "sse41", "avx2", "avx512", "neon",
    "wasm_simd128")
)
```

## Arguments

- backend:

  One of `"auto"`, `"scalar"`, `"sse2"`, `"sse41"`, `"avx2"`,
  `"avx512"`, `"neon"`, or `"wasm_simd128"`.

## Value

The selected backend, invisibly. For `"auto"`, this is the backend
chosen from the compiled and CPU-supported set.

## Examples

``` r
old <- simd_backend()
simd_set_backend("scalar")
simd_set_backend("auto")
```
