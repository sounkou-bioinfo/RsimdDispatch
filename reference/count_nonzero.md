# Count non-zero bytes with the selected SIMD backend

Demonstration kernel for the runtime dispatch template.
`count_nonzero()` counts bytes that are not `00` in a raw vector using
the currently selected backend. The default backend is `"auto"`, which
selects the best compiled backend supported by the current CPU/runtime.

## Usage

``` r
count_nonzero(x)
```

## Arguments

- x:

  A raw vector.

## Value

A `double` scalar giving the count of non-zero bytes. The return type is
`double` rather than `integer` to accommodate vectors longer than
`.Machine$integer.max` without overflow.

## Examples

``` r
count_nonzero(as.raw(c(0, 1, 0, 2)))
#> [1] 2
```
