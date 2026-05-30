#ifndef SD_CPU_FEATURES_H
#define SD_CPU_FEATURES_H

#ifdef __cplusplus
extern "C" {
#endif

/* --------------------------------------------------------------------------
 * Runtime CPU feature detection
 *
 * Each function returns 1 if the CPU/runtime supports the named ISA
 * extension, 0 otherwise.  On non-x86 platforms the x86 functions always
 * return 0.  Detection is based on CPUID (x86), compile-time macros
 * (NEON/AArch64), or __builtin_cpu_supports where available.
 * -------------------------------------------------------------------------- */

int sd_cpu_has_sse2(void);
int sd_cpu_has_sse41(void);
int sd_cpu_has_avx2(void);
int sd_cpu_has_avx512(void);
int sd_cpu_has_neon(void);
int sd_cpu_has_wasm_simd128(void);

/* --------------------------------------------------------------------------
 * Target platform strings
 *
 * These return compile-time constant strings identifying the build target.
 * They do NOT reflect the runtime CPU; use the functions above for that.
 * -------------------------------------------------------------------------- */

/* CPU architecture as reported by the compiler (e.g. "x86_64", "aarch64"). */
const char *sd_target_arch(void);

/* Operating system family (e.g. "linux", "macos", "windows", "emscripten"). */
const char *sd_target_os(void);

#ifdef __cplusplus
}
#endif

#endif
