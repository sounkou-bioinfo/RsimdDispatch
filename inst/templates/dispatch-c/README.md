# RsimdDispatch C template

This directory is a copyable starting point for pure-C runtime SIMD dispatch in
an R package.

The important pattern is:

- compile R API, CPU detection, and dispatch files through ordinary `src/Makevars`;
- stage scalar and optional SIMD kernel objects during `configure`;
- select only a backend that is both compiled and supported by the current CPU/runtime;
- switch guarded function pointers when `simd_set_backend()` is called.

Copy with:

```r
RsimdDispatch::use_simd_dispatch("/path/to/pkg", pkg = "YourPackage", prefix = "ypkg")
```

The helper substitutes the package name and C symbol prefixes. Then replace the
example `count_nonzero()` kernel with your package-specific kernel signature.
See `CHECKLIST.md` if you copy files manually.
