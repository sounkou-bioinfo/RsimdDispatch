# Report runtime SIMD dispatch diagnostics

Returns the requested backend, selected backend, compiled backends,
CPU-supported backends, operation-level backend availability,
operation-level selected backends, SIMDe-native backends, target
information, and SIMDe provenance compiled into the shared library.
Calling this initializes the lazy auto-dispatch selection if it has not
already been initialized.

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
#>  [1] "dispatch_mode"               "requested_backend"          
#>  [3] "selected_backend"            "compiled_backends"          
#>  [5] "cpu_supported_backends"      "available_backends"         
#>  [7] "simde_native_backends"       "operations"                 
#>  [9] "operation_backends"          "operation_selected_backends"
#> [11] "cpu_sse2"                    "cpu_sse41"                  
#> [13] "cpu_avx2"                    "cpu_avx512"                 
#> [15] "cpu_neon"                    "cpu_wasm_simd128"           
#> [17] "target_arch"                 "target_os"                  
#> [19] "simde_version"               "simde_commit"               
```
