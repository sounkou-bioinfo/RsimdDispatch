# RsimdDispatch C template

This directory is a copyable starting point for pure-C runtime SIMD dispatch in
an R package.

The important pattern is:

- compile R API, CPU detection, and dispatch files with baseline flags only;
- compile each SIMD kernel translation unit with only the flags it needs;
- select only a backend that is both compiled and supported by the current CPU;
- switch guarded function pointers when `simd_set_backend()` is called.

Copy with:

```r
RsimdDispatch::use_simd_dispatch("/path/to/pkg")
```

Then rename the `rsd_` and `RC_` symbols for your package and kernel signatures.
