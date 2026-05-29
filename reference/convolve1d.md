# Full one-dimensional convolution with the selected SIMD backend

Demonstration numeric kernel for the runtime dispatch template.
`convolve1d()` computes the same full convolution as a simple
nested-loop R definition. For each pair of positions it adds
`a[i] * b[j]` to `out[i + j - 1]`. SIMD backends vectorize the inner
multiply-add over `b` and the shifted output window.

## Usage

``` r
convolve1d(a, b)
```

## Arguments

- a, b:

  Numeric vectors.

## Value

A numeric vector of length `length(a) + length(b) - 1`, or `numeric(0)`
when either input is empty.

## Examples

``` r
convolve1d(c(1, 2, 3), c(10, 100))
#> [1]  10 120 230 300
```
