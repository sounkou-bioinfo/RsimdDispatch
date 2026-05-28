# Changelog

## RsimdDispatch 0.1.0

Initial release.

- Provides a working runtime SIMD dispatch example for R packages, using
  one shared library with scalar, SSE2, SSE4.1, AVX2, AVX-512, and NEON
  translation units where supported by the compiler and CPU.
- Exports
  [`count_nonzero()`](https://sounkou-bioinfo.github.io/RsimdDispatch/reference/count_nonzero.md),
  [`simd_set_backend()`](https://sounkou-bioinfo.github.io/RsimdDispatch/reference/simd_set_backend.md),
  [`simd_backend()`](https://sounkou-bioinfo.github.io/RsimdDispatch/reference/simd_backend.md),
  and
  [`simd_info()`](https://sounkou-bioinfo.github.io/RsimdDispatch/reference/simd_info.md)
  to demonstrate safe same-process backend selection and diagnostics.
- Adds
  [`use_simd_dispatch()`](https://sounkou-bioinfo.github.io/RsimdDispatch/reference/simd_dispatch_template_path.md)
  and
  [`simd_dispatch_template_path()`](https://sounkou-bioinfo.github.io/RsimdDispatch/reference/simd_dispatch_template_path.md)
  for package authors who want to copy and adapt the C dispatch
  scaffold.
- Vendors the header-only SIMDe library for downstream packages through
  `LinkingTo: RsimdDispatch`.
- Adds
  [`simde_info()`](https://sounkou-bioinfo.github.io/RsimdDispatch/reference/simde_info.md)
  and SIMDe fields in
  [`simd_info()`](https://sounkou-bioinfo.github.io/RsimdDispatch/reference/simd_info.md)
  to report the vendored SIMDe version and pinned upstream commit.
