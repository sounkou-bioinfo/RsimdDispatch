# Configure an R package for C runtime SIMD dispatch

`use_simd_dispatch()` copies the dispatch scaffold into an R package and
performs the package-name and C-prefix substitutions needed for a
working package. It writes package files, updates `DESCRIPTION`,
`.Rbuildignore`, and `.gitignore`, and returns the copied paths
invisibly.

## Usage

``` r
simd_dispatch_template_path()

use_simd_dispatch(
  path = ".",
  pkg = NULL,
  prefix = NULL,
  overwrite = FALSE,
  quiet = FALSE
)
```

## Arguments

- path:

  Package root where the template should be copied.

- pkg:

  R package name. If `NULL`, the name is read from `DESCRIPTION`.

- prefix:

  C symbol prefix used to replace `rsd_` in the copied sources. The
  default is a sanitized lowercase package name.

- overwrite:

  Whether to overwrite existing files.

- quiet:

  Whether to suppress progress messages.

## Value

Invisibly returns copied file paths.

## Developer utility

This function is intended for package authors. It is not needed at
runtime by users of packages that already include generated dispatch
code.

## Examples

``` r
simd_dispatch_template_path()
#> [1] "/home/runner/work/_temp/Library/RsimdDispatch/templates/dispatch-c"
```
