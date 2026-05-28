# Changelog

## RsimdDispatch 0.1.2

- Add WebAssembly SIMD128 (`wasm_simd128`) staged-kernel support for
  Emscripten/webR builds using SIMDe’s `<simde/wasm/simd128.h>` backend
  and `SIMDE_WASM_SIMD128_NATIVE` configure probing.
- Extend
  [`simd_info()`](https://sounkou-bioinfo.github.io/RsimdDispatch/reference/simd_info.md)
  diagnostics with `cpu_wasm_simd128` and include `wasm_simd128` in
  compiled, available, and SIMDe-native backend sets when built by a
  SIMD128-capable WebAssembly toolchain.
- Add webR check tooling based on the local `rwasm::build()` + Node
  `webr` workflow used by related packages.
- Generate explicit SIMDe include flags in `src/Makevars` and quote
  configure include paths so copied templates are more robust in
  downstream libraries.
- Eagerly initialize dispatch at package load, exercise copied-template
  runtime behavior in CI, and remove legacy comma-separated backend-list
  C helpers.
- Add a dispatched
  [`convolve3()`](https://sounkou-bioinfo.github.io/RsimdDispatch/reference/convolve3.md)
  numeric three-tap convolution demo and include it in tests,
  documentation, webR checks, and evaluated benchmarks.

## RsimdDispatch 0.1.1 (2026-05-28)

- Stage scalar and optional SIMD kernel objects during `configure`, then
  link them through generated `src/Makevars` with the baseline R API,
  CPU feature detection, and dispatcher code.
- Move demo kernels from `src/` to `tools/kernels/` so copied templates
  keep build-time kernel sources separate from ordinary R package `src/`
  files.
- Reword `DESCRIPTION` to avoid incoming spell-check notes for backend
  acronym names while keeping detailed backend names in user-facing
  documentation.

## RsimdDispatch 0.1.0

Initial release.

- Provides a working runtime SIMD dispatch example for R packages, using
  one shared library with scalar, SSE2, SSE4.1, AVX2, AVX-512, and NEON
  kernel objects where supported by the compiler and CPU.
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
