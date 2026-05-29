# RsimdDispatch 0.1.2

* Add WebAssembly SIMD128 (`wasm_simd128`) staged-kernel support for
  Emscripten/webR builds using SIMDe's `<simde/wasm/simd128.h>` backend and
  `SIMDE_WASM_SIMD128_NATIVE` configure probing.
* Extend `simd_info()` diagnostics with `cpu_wasm_simd128` and include
  `wasm_simd128` in compiled, available, and SIMDe-native backend sets when
  built by a SIMD128-capable WebAssembly toolchain.
* Add webR check tooling based on the local `rwasm::build()` + Node `webr`
  workflow used by related packages.
* Generate explicit SIMDe include flags in `src/Makevars` and quote configure
  include paths so copied templates are more robust in downstream libraries.
* Eagerly initialize dispatch at package load, exercise copied-template runtime
  behavior in CI, and remove legacy comma-separated backend-list C helpers.
* Switch dispatch from one function pointer per operation to a backend `RsdOps`
  operation table, use `RSD_DISPATCH_OPS()` to generate operation typedefs,
  fields, backend declarations, table initializers, and wrappers, and use an
  X-macro list for `.Call` routine registration. Backend diagnostics now reuse
  the C backend metadata table, and the R setter no longer hardcodes backend
  names. This makes adding several dispatched operations substantially less
  brittle.
* Add a dispatched `convolve1d()` full one-dimensional convolution demo, using
  SIMDe inner-loop multiply-add kernels for numeric vectors, and include it in
  tests, documentation, webR checks, and evaluated benchmarks.
* Document the current extension path and the next scalable step for many
  operations: a NumKong-style operation/dtype/capability finder while retaining
  RsimdDispatch's staged-object R package build model.

# RsimdDispatch 0.1.1 (2026-05-28)

* Stage scalar and optional SIMD kernel objects during `configure`, then link
  them through generated `src/Makevars` with the baseline R API, CPU feature
  detection, and dispatcher code.
* Move demo kernels from `src/` to `tools/kernels/` so copied templates keep
  build-time kernel sources separate from ordinary R package `src/` files.
* Reword `DESCRIPTION` to avoid incoming spell-check notes for backend acronym
  names while keeping detailed backend names in user-facing documentation.

# RsimdDispatch 0.1.0

Initial release.

* Provides a working runtime SIMD dispatch example for R packages, using one
  shared library with scalar, SSE2, SSE4.1, AVX2, AVX-512, and NEON kernel
  objects where supported by the compiler and CPU.
* Exports `count_nonzero()`, `simd_set_backend()`, `simd_backend()`, and
  `simd_info()` to demonstrate safe same-process backend selection and
  diagnostics.
* Adds `use_simd_dispatch()` and `simd_dispatch_template_path()` for package
  authors who want to copy and adapt the C dispatch scaffold.
* Vendors the header-only SIMDe library for downstream packages through
  `LinkingTo: RsimdDispatch`.
* Adds `simde_info()` and SIMDe fields in `simd_info()` to report the vendored
  SIMDe version and pinned upstream commit.
