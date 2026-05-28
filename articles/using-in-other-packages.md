# Using RsimdDispatch in Other Packages

Downstream packages use `RsimdDispatch` as a template and header
provider. The usual workflow is:

1.  call
    [`use_simd_dispatch()`](https://sounkou-bioinfo.github.io/RsimdDispatch/reference/simd_dispatch_template_path.md)
    from the package root;
2.  replace the demo
    [`count_nonzero()`](https://sounkou-bioinfo.github.io/RsimdDispatch/reference/count_nonzero.md)
    kernels with package-specific kernels;
3.  keep CPU detection and dispatch files compiled with baseline flags;
4.  compile each ISA file with only the flags it needs.

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
src/Makevars.in
src/Makevars.win.in
src/cpu_features.c
src/simd_dispatch.c
src/kernel_scalar.c
src/kernel_avx2.c
src/kernel_avx512.c
src/kernel_neon.c
```

## Replace the demo kernel

The demo kernel has this shape:

``` c
typedef size_t (*rsd_count_nonzero_fn)(const uint8_t *x, size_t n);
```

For a real package, replace the demo kernel signature with your own and
keep the same structure:

``` text
R API wrapper        baseline flags
CPU feature checks   baseline flags
dispatch table       baseline flags
scalar kernel        baseline flags
SSE/AVX/NEON files   per-file ISA flags
```

Do not put `-mavx2`, `-mavx512*`, or `-march=native` in global package
flags. The dispatcher must remain safe on baseline CPUs.

## Build

The generated `configure` checks compiler support and writes
`src/config.h` plus `src/Makevars`. Runtime CPU support is checked later
by the installed package:

``` sh
R CMD INSTALL .
```
