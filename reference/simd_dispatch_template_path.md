# Locate or copy the C runtime dispatch template

`simd_dispatch_template_path()` returns the installed template
directory. `use_simd_dispatch()` copies the template into a package
directory. It is a deliberately small helper; downstream packages should
review and rename the generated symbols for their own kernels.

## Usage

``` r
simd_dispatch_template_path()

use_simd_dispatch(path = ".", overwrite = FALSE)
```

## Arguments

- path:

  Package root where the template should be copied.

- overwrite:

  Whether to overwrite existing files.

## Value

`use_simd_dispatch()` invisibly returns copied file paths.

## Examples

``` r
simd_dispatch_template_path()
#> [1] "/home/runner/work/_temp/Library/RsimdDispatch/templates/dispatch-c"
```
