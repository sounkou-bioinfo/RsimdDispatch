# Three-tap numeric convolution with the selected SIMD backend

Demonstration numeric kernel for the runtime dispatch template.
`convolve3()` computes a valid three-tap convolution/FIR pass over a
numeric vector using the currently selected backend:
`out[i] = kernel[1] * x[i] + kernel[2] * x[i + 1] + kernel[3] * x[i + 2]`.

## Usage

``` r
convolve3(x, kernel = c(0.25, 0.5, 0.25))
```

## Arguments

- x:

  A numeric vector.

- kernel:

  Numeric vector of length 3. The default is a symmetric smoothing
  kernel.

## Value

A numeric vector of length `max(length(x) - 2, 0)`.

## Examples

``` r
convolve3(c(1, 2, 3, 4), c(1, 0, -1))
#> [1] -2 -2
```
