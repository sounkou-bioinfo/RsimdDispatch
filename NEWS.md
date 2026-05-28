# RsimdDispatch 0.1.0

Initial release.

* Provides a working runtime SIMD dispatch example for R packages, using one
  shared library with scalar, SSE2, SSE4.1, AVX2, AVX-512, and NEON translation
  units where supported by the compiler and CPU.
* Exports `count_nonzero()`, `simd_set_backend()`, `simd_backend()`, and
  `simd_info()` to demonstrate safe same-process backend selection and
  diagnostics.
* Adds `use_simd_dispatch()` and `simd_dispatch_template_path()` for package
  authors who want to copy and adapt the C dispatch scaffold.
* Vendors the header-only SIMDe library for downstream packages through
  `LinkingTo: RsimdDispatch`.
* Adds `simde_info()` and SIMDe fields in `simd_info()` to report the vendored
  SIMDe version and pinned upstream commit.
