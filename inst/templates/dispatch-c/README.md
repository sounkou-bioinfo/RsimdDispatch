# RsimdDispatch C template

This directory is a copyable starting point for pure-C runtime SIMD dispatch in
an R package.

The important pattern is:

- compile R API, CPU detection, and dispatch files through ordinary `src/Makevars`;
- stage scalar and optional SIMD kernel objects during `configure`;
- keep the staged-kernel registration ABI in `tools/simdDispatch/kernels/kernel_api.h`,
  separate from private dispatch headers in `src/`;
- select only a backend that is both compiled and supported by the current CPU/runtime;
- rebuild a guarded resolved operation table when `simd_set_backend()` is called.

Copy with:

```r
RsimdDispatch::use_simd_dispatch("/path/to/pkg", pkg = "YourPackage", prefix = "ypkg")
```

The helper substitutes the package name and C symbol prefixes. Then replace the
example `count_nonzero()` and `convolve1d()` kernels with your package-specific
kernel signatures. Backend files expose `SdKernelDef` tables and register only
the operation slots they implement; omit a table row when a backend
intentionally does not implement an operation. See `CHECKLIST.md` if you copy
files manually.
