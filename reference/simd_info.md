# Report runtime SIMD dispatch diagnostics

Returns the requested backend, selected backend, compiled backends,
CPU-supported backends, SIMDe-native backends, target information, and
SIMDe provenance compiled into the shared library. Calling this
initializes the lazy auto-dispatch selection if it has not already been
initialized.

## Usage

``` r
simd_info()
```

## Value

A named list of dispatch and CPU feature diagnostics. Backend-set
entries are character vectors, not comma-separated strings.

## Examples

``` r
names(simd_info())
#>  [1] "dispatch_mode"          "requested_backend"      "selected_backend"      
#>  [4] "compiled_backends"      "cpu_supported_backends" "available_backends"    
#>  [7] "simde_native_backends"  "cpu_sse2"               "cpu_sse41"             
#> [10] "cpu_avx2"               "cpu_avx512"             "cpu_neon"              
#> [13] "cpu_wasm_simd128"       "target_arch"            "target_os"             
#> [16] "simde_version"          "simde_commit"          
```
