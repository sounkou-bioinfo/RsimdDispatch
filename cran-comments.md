## R CMD check results

Local check:

```text
R CMD check --as-cran --no-manual RsimdDispatch_0.1.1.tar.gz
```

Expected notes:

- New submission.

Build-system note:

- The package intentionally stages optional SIMD kernel objects during
  `configure`; the generated `src/Makevars` links those objects with baseline
  R API, CPU feature detection, and dispatcher code. Runtime selection only
  allows compiled and CPU-supported backends.

Additional context:

- The package bundles the header-only 'SIMDe' library, which accounts for most
  of the installed size under `inst/include/simde`; the compressed source
  package remains small.

## New submission

This is a development package and has not yet been submitted to CRAN.
