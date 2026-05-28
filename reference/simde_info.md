# Report vendored SIMDe provenance

`simde_info()` reports the version, upstream repository, pinned commit,
and commit date for the bundled header-only SIMDe library.

## Usage

``` r
simde_info()
```

## Value

A named list of character scalars describing the vendored SIMDe copy.

## Examples

``` r
simde_info()[c("version", "commit")]
#> $version
#> [1] "0.8.4"
#> 
#> $commit
#> [1] "f3e8262173b7089db9a9d57a9ecef8dd07ad9c97"
#> 
```
