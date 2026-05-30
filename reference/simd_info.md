# Report runtime SIMD dispatch diagnostics

Returns the requested backend, selected backend, compiled backends,
CPU-supported backends, operation-level backend availability,
operation-level selected backends, SIMDe-native backends, and target
architecture information. Calling this initializes the lazy
auto-dispatch selection if it has not already been initialized. For
SIMDe provenance (version, commit, date) use
[`simde_info()`](https://sounkou-bioinfo.github.io/RsimdDispatch/reference/simde_info.md).

## Usage

``` r
simd_info()
```

## Value

A named list with the following elements:

- `dispatch_mode`:

  Character scalar. Internal dispatch implementation strategy string
  (stable across patch releases).

- `requested_backend`:

  Character scalar. The last value passed to
  [`simd_set_backend()`](https://sounkou-bioinfo.github.io/RsimdDispatch/reference/simd_set_backend.md),
  or `"auto"` if not explicitly set.

- `selected_backend`:

  Character scalar. Summary of the currently active backend: a backend
  name, `"mixed"`, or `"unavailable"`.

- `compiled_backends`:

  Character vector. Backends compiled into the shared library (not
  necessarily supported by the current CPU).

- `cpu_supported_backends`:

  Character vector. Backends whose required CPU features are present at
  runtime.

- `available_backends`:

  Character vector. Backends that are both compiled and CPU-supported
  (i.e. usable).

- `simde_native_backends`:

  Character vector. Backends for which the compiler emitted native
  intrinsics rather than SIMDe emulation.

- `operations`:

  Character vector. Names of registered operations (e.g.
  `"count_nonzero"`, `"convolve1d"`).

- `operation_backends`:

  Named list of character vectors. For each operation, the backends that
  provide a kernel for it.

- `operation_selected_backends`:

  Named character vector. For each operation, the backend currently
  selected for dispatch (or `NA` if none is resolved).

- `cpu_sse2`:

  Logical scalar. `TRUE` if SSE2 is detected at runtime.

- `cpu_sse41`:

  Logical scalar. `TRUE` if SSE4.1 is detected.

- `cpu_avx2`:

  Logical scalar. `TRUE` if AVX2 is detected.

- `cpu_avx512`:

  Logical scalar. `TRUE` if AVX-512 (F/BW/VL) is detected.

- `cpu_neon`:

  Logical scalar. `TRUE` if NEON/AdvSIMD is detected.

- `cpu_wasm_simd128`:

  Logical scalar. `TRUE` if WASM SIMD128 is available.

- `target_arch`:

  Character scalar. Compiler target CPU architecture (e.g. `"x86_64"`,
  `"aarch64"`).

- `target_os`:

  Character scalar. Compiler target OS family (e.g. `"linux"`,
  `"macos"`, `"windows"`, `"emscripten"`).

## Examples

``` r
names(simd_info())
#>  [1] "dispatch_mode"               "requested_backend"          
#>  [3] "selected_backend"            "compiled_backends"          
#>  [5] "cpu_supported_backends"      "available_backends"         
#>  [7] "simde_native_backends"       "operations"                 
#>  [9] "operation_backends"          "operation_selected_backends"
#> [11] "cpu_sse2"                    "cpu_sse41"                  
#> [13] "cpu_avx2"                    "cpu_avx512"                 
#> [15] "cpu_neon"                    "cpu_wasm_simd128"           
#> [17] "target_arch"                 "target_os"                  
```
