## R CMD check results

Local check:

```text
R CMD check --as-cran --no-manual RsimdDispatch_0.1.0.tar.gz
```

Expected notes:

- New submission.
- The package intentionally compiles SIMD translation units with checked,
  per-file ISA flags such as `-mavx2` and `-mavx512*`; the R API, CPU feature
  detection, and dispatcher are compiled without those flags, and runtime
  selection only allows compiled and CPU-supported backends.

Additional context:

- The package bundles the header-only 'SIMDe' library, which accounts for most
  of the installed size under `inst/include/simde`; the compressed source
  package remains small.

## New submission

This is a development package and has not yet been submitted to CRAN.
