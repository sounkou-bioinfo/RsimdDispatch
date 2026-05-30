# Using RsimdDispatch in Other Packages

Downstream packages use `RsimdDispatch` as a template and header
provider. The usual workflow is:

1.  call
    [`use_simd_dispatch()`](https://sounkou-bioinfo.github.io/RsimdDispatch/reference/simd_dispatch_template_path.md)
    from the package root;
2.  replace the demo
    [`count_nonzero()`](https://sounkou-bioinfo.github.io/RsimdDispatch/reference/count_nonzero.md)
    and
    [`convolve1d()`](https://sounkou-bioinfo.github.io/RsimdDispatch/reference/convolve1d.md)
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
tools/simdDispatch/kernels/kernel_scalar.c
tools/simdDispatch/kernels/kernel_sse2.c
tools/simdDispatch/kernels/kernel_sse41.c
tools/simdDispatch/kernels/kernel_avx2.c
tools/simdDispatch/kernels/kernel_avx512.c
tools/simdDispatch/kernels/kernel_neon.c
tools/simdDispatch/kernels/kernel_wasm_simd128.c
tools/simdDispatch/kernels/kernel_common.h
src/Makevars.in
src/Makevars.win.in
```

## Replace the demo kernels

The demo operations are registered by each backend file. For example,
the AVX2 staged source registers both demo operations:

``` c
static const SdKernelDef sd_avx2_kernels[] = {
    {SD_OP_COUNT_NONZERO, SD_SIG_RAW_COUNT, sd_count_nonzero_avx2_invoke},
    {SD_OP_CONVOLVE1D, SD_SIG_F64_CONVOLVE, sd_convolve1d_avx2_invoke},
    SD_KERNEL_DEF_END
};

void sd_register_avx2(SdDispatchBuilder *builder) {
    sd_register_kernel_table(builder, sd_avx2_kernels);
}
```

SSE2 and SSE4.1 currently register only
[`count_nonzero()`](https://sounkou-bioinfo.github.io/RsimdDispatch/reference/count_nonzero.md),
which demonstrates operation-level backend support. A backend
intentionally omits an operation by not registering that slot.
Explicitly selecting such a backend is allowed, but calling the
unsupported operation errors clearly; `"auto"` skips that backend for
the unsupported operation.

For a real package, replace the demo operation signatures with your own
and keep the same structure. Staged kernel files include
`tools/simdDispatch/kernels/kernel_api.h`, not private headers from
`src/`; the dispatch core implements that kernel-facing registration
ABI.

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

## Extending the number of operations

The staged build model already scales across operations: if you add
another function to each existing
`tools/simdDispatch/kernels/kernel_*.c` file, the same staged object for
that backend contains the new implementation. For a small number of
operations, keep the dispatch code explicit and add:

1.  an operation ID, signature ID, call-frame type, and kernel
    definition entry type support in
    `tools/simdDispatch/kernels/kernel_api.h`;
2.  one implementation plus a small `void *` call-frame thunk per
    backend file under `tools/simdDispatch/kernels/`, following the
    naming convention `sd_<operation>_<backend>()`;
3.  one `SdKernelDef` row in each backend file that implements the
    operation;
4.  the `.Call` wrapper in `src/r_api.c`, which marshals R objects into
    the operation call frame and calls `sd_dispatch_invoke()`;
5.  the native registration entry in `src/registration.c`;
6.  the R wrapper, tests, and documentation.

Backend metadata and kernel availability are table-driven, similar in
spirit to R’s native routine registration tables. Operation
implementation ownership stays close to each backend source file, while
`tools/simdDispatch/simd_dispatch.c` consumes only opaque operation IDs
and generic kernel-definition rows.

For packages with many public operations, keep a single operation
catalog that can generate the repetitive dispatch metadata, `.Call`
registration, and R wrappers. Optimized backend kernels can remain
hand-written, but operation names, argument lists, and exported wrapper
metadata should not be duplicated by hand.

## Build

The generated `configure` checks compiler support, writes `src/config.h`
plus `src/Makevars`, and stages selected kernel objects in
`src/rsd-kernels/`. Runtime CPU support is checked later by the
installed package:

``` sh
R CMD INSTALL .
```
