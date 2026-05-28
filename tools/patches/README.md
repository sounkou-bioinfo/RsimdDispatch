# Vendor patches

Patches in this directory are applied by `tools/vendor-simde.R` after copying the
pinned upstream SIMDe include tree into `inst/include/simde`.

Keep patches small and CRAN-motivated. Each patch should be reproducible with:

```sh
git diff -- inst/include/simde > tools/patches/<name>.patch
```
