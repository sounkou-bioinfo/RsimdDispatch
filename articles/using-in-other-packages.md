# Using RsimdDispatch in Other Packages

Downstream packages use `RsimdDispatch` as a template and header
provider. The usual workflow is:

1.  call
    [`use_simd_dispatch()`](https://sounkou-bioinfo.github.io/RsimdDispatch/reference/simd_dispatch_template_path.md)
    from the package root;
2.  replace the demo
    [`count_nonzero()`](https://sounkou-bioinfo.github.io/RsimdDispatch/reference/count_nonzero.md)
    and
    [`convolve3()`](https://sounkou-bioinfo.github.io/RsimdDispatch/reference/convolve3.md)
    kernels with package-specific kernels;
3.  keep CPU detection, dispatch, and R API files compiled by R’s
    ordinary `src/Makevars` path;
4.  let `configure` stage scalar and ISA-specific kernel objects before
    linking one shared library.

[`use_simd_dispatch()`](https://sounkou-bioinfo.github.io/RsimdDispatch/reference/simd_dispatch_template_path.md)
updates `DESCRIPTION`, adding `RsimdDispatch` to `LinkingTo`, and copies
package-specific scaffold files. It does not add a runtime dependency on
`RsimdDispatch`. The `LinkingTo` entry makes bundled SIMDe headers
available to C code:

``` c
#include <simde/x86/avx2.h>
#include <simde/arm/neon.h>
```

## Copy the scaffold

From the downstream package root:

``` r

RsimdDispatch::use_simd_dispatch(pkg = "MyPackage", prefix = "mypkg")
```

If `pkg` is omitted, the package name is read from `DESCRIPTION`. If
`prefix` is omitted, a lowercase C identifier prefix is derived from the
package name. The helper performs the important substitutions, including
`@useDynLib`, `R_init_<Package>()`, `rsd_`, `RSD_`, and `RC_` symbols.

The copied scaffold includes:

``` text
configure
configure.win
cleanup
tools/configure-simd-dispatch.sh
tools/kernels/kernel_scalar.c
tools/kernels/kernel_sse2.c
tools/kernels/kernel_sse41.c
tools/kernels/kernel_avx2.c
tools/kernels/kernel_avx512.c
tools/kernels/kernel_neon.c
tools/kernels/kernel_wasm_simd128.c
src/Makevars.in
src/Makevars.win.in
src/cpu_features.c
src/simd_dispatch.c
```

## Replace the demo kernels

The demo operations are collected in a backend operation table:

``` c
typedef struct RsdOps {
    rsd_count_nonzero_fn count_nonzero;
    rsd_convolve3_fn convolve3;
} RsdOps;
```

For a real package, replace the demo operation signatures with your own
and keep the same structure:

``` text
R API wrapper        ordinary src/Makevars compilation
CPU feature checks   ordinary src/Makevars compilation
dispatch table       ordinary src/Makevars compilation
scalar kernel        staged by configure under src/rsd-kernels/
SSE/AVX/NEON/wasm files   staged by configure as optional objects under src/rsd-kernels/
```

Do not put `-mavx2`, `-mavx512*`, `-msimd128`, or `-march=native` in
global package flags. The configure helper keeps ISA flags local to
optional staged kernel objects, and the dispatcher remains safe on
baseline CPUs.

## Build

The generated `configure` checks compiler support, writes `src/config.h`
plus `src/Makevars`, and stages selected kernel objects in
`src/rsd-kernels/`. Runtime CPU support is checked later by the
installed package:

``` sh
R CMD INSTALL .
```
