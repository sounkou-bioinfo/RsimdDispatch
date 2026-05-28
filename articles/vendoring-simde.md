# Maintainer Workflow: Vendoring SIMDe

This vignette is for maintainers of `RsimdDispatch` itself. Downstream
packages should normally use `LinkingTo: RsimdDispatch` and the copied
dispatch template; they do not need to run the vendoring scripts.

`RsimdDispatch` vendors the full header-only SIMDe include tree in
`inst/include/simde`. The pinned upstream checkout is recorded in:

``` r

readLines(system.file("vendor", "simde", "VERSION", package = "RsimdDispatch"))
#> [1] "Component: SIMDe"                                    
#> [2] "Repository: https://github.com/simd-everywhere/simde"
#> [3] "Commit: f3e8262173b7089db9a9d57a9ecef8dd07ad9c97"    
#> [4] "Date: 2026-05-10"                                    
#> [5] "Vendored-include-tree: inst/include/simde"           
#> [6] "License-file: inst/vendor/simde/COPYING"
```

The update flow is explicit:

``` sh
Rscript tools/vendor-simde.R
Rscript tools/update-authors.R
```

`tools/vendor-simde.R` checks out the pinned SIMDe commit, copies the
full `simde/` include tree into `inst/include/simde`, and stores
upstream provenance under `inst/vendor/simde`.

`tools/update-authors.R` regenerates:

``` text
inst/AUTHORS
inst/LICENCE.note
```

These files document bundled SIMDe authorship, contributors, the
upstream license file, and the vendored commit. Local changes to
vendored headers should not be made silently; update the vendoring
script or record patches explicitly.
