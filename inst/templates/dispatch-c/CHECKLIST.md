# RsimdDispatch template checklist

If you copy files manually, update at least these package-specific values:

- `R/dispatch.R`: `@useDynLib RsimdDispatch` must use your package name.
- `src/registration.c`: `R_init_RsimdDispatch()` must become `R_init_<Package>()`.
- `tools/configure-simd-dispatch.sh`: set `RSD_LOG_PREFIX` or replace the log prefix.
- Review exported R names (`count_nonzero()`, `convolve1d()`, `simd_set_backend()`, `simd_backend()`, `simd_info()`).
- Replace the demo `count_nonzero()` and `convolve1d()` kernel signatures in `tools/kernels/` with your package kernel signatures.

Prefer using:

```r
RsimdDispatch::use_simd_dispatch(pkg = "YourPackage", prefix = "ypkg")
```

which performs the package-name and prefix substitutions for you.
