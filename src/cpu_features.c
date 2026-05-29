#include "cpu_features.h"

#include <stdint.h>

#if defined(__EMSCRIPTEN__)
#define RSD_TARGET_OS "emscripten"
#elif defined(_WIN32)
#define RSD_TARGET_OS "windows"
#elif defined(__APPLE__)
#define RSD_TARGET_OS "darwin"
#elif defined(__linux__)
#define RSD_TARGET_OS "linux"
#else
#define RSD_TARGET_OS "unknown"
#endif

#if defined(__x86_64__) || defined(_M_X64)
#define RSD_TARGET_ARCH "x86_64"
#elif defined(__i386__) || defined(_M_IX86)
#define RSD_TARGET_ARCH "x86"
#elif defined(__aarch64__) || defined(_M_ARM64)
#define RSD_TARGET_ARCH "aarch64"
#elif defined(__arm__) || defined(_M_ARM)
#define RSD_TARGET_ARCH "arm"
#elif defined(__wasm32__)
#define RSD_TARGET_ARCH "wasm32"
#elif defined(__wasm64__)
#define RSD_TARGET_ARCH "wasm64"
#else
#define RSD_TARGET_ARCH "unknown"
#endif

const char *rsd_target_arch(void) {
    return RSD_TARGET_ARCH;
}

const char *rsd_target_os(void) {
    return RSD_TARGET_OS;
}

#if defined(__x86_64__) || defined(__i386__) || defined(_M_X64) || defined(_M_IX86)
#define RSD_X86 1
#else
#define RSD_X86 0
#endif

#if RSD_X86

typedef struct RsdCpuRegs {
    uint32_t eax;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
} RsdCpuRegs;

#if defined(_MSC_VER)
#include <intrin.h>
static int rsd_cpuid(uint32_t leaf, uint32_t subleaf, RsdCpuRegs *out) {
    int regs[4];
    __cpuidex(regs, (int)leaf, (int)subleaf);
    out->eax = (uint32_t)regs[0];
    out->ebx = (uint32_t)regs[1];
    out->ecx = (uint32_t)regs[2];
    out->edx = (uint32_t)regs[3];
    return 1;
}

static uint64_t rsd_xgetbv(uint32_t index) {
    return (uint64_t)_xgetbv(index);
}
#elif defined(__GNUC__) || defined(__clang__)
#if defined(__has_include)
#if __has_include(<cpuid.h>)
#define RSD_HAVE_CPUID_H 1
#endif
#endif
#ifndef RSD_HAVE_CPUID_H
#define RSD_HAVE_CPUID_H 0
#endif
#if RSD_HAVE_CPUID_H
#include <cpuid.h>
static int rsd_cpuid(uint32_t leaf, uint32_t subleaf, RsdCpuRegs *out) {
    unsigned int eax = 0, ebx = 0, ecx = 0, edx = 0;
    if (!__get_cpuid_count(leaf, subleaf, &eax, &ebx, &ecx, &edx)) {
        out->eax = out->ebx = out->ecx = out->edx = 0;
        return 0;
    }
    out->eax = (uint32_t)eax;
    out->ebx = (uint32_t)ebx;
    out->ecx = (uint32_t)ecx;
    out->edx = (uint32_t)edx;
    return 1;
}
#else
static int rsd_cpuid(uint32_t leaf, uint32_t subleaf, RsdCpuRegs *out) {
    uint32_t eax = 0, ebx = 0, ecx = 0, edx = 0;
#if defined(__i386__) && defined(__PIC__)
    __asm__ volatile("xchgl %%ebx, %1; cpuid; xchgl %%ebx, %1"
                     : "=a"(eax), "=&r"(ebx), "=c"(ecx), "=d"(edx)
                     : "0"(leaf), "2"(subleaf));
#else
    __asm__ volatile("cpuid"
                     : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
                     : "a"(leaf), "c"(subleaf));
#endif
    out->eax = eax;
    out->ebx = ebx;
    out->ecx = ecx;
    out->edx = edx;
    return 1;
}
#endif

static uint64_t rsd_xgetbv(uint32_t index) {
    uint32_t eax = 0;
    uint32_t edx = 0;
    __asm__ volatile("xgetbv" : "=a"(eax), "=d"(edx) : "c"(index));
    return ((uint64_t)edx << 32) | eax;
}
#else
static int rsd_cpuid(uint32_t leaf, uint32_t subleaf, RsdCpuRegs *out) {
    (void)leaf;
    (void)subleaf;
    out->eax = out->ebx = out->ecx = out->edx = 0;
    return 0;
}

static uint64_t rsd_xgetbv(uint32_t index) {
    (void)index;
    return 0;
}
#endif

static int rsd_bit(uint32_t value, unsigned int bit) {
    return (value & (UINT32_C(1) << bit)) != 0;
}

static int rsd_max_leaf_at_least(uint32_t leaf) {
    RsdCpuRegs regs;
    if (!rsd_cpuid(0, 0, &regs)) {
        return 0;
    }
    return regs.eax >= leaf;
}

static int rsd_os_supports_avx(RsdCpuRegs leaf1) {
    if (!rsd_bit(leaf1.ecx, 26) || !rsd_bit(leaf1.ecx, 27)) {
        return 0;
    }
    return (rsd_xgetbv(0) & UINT64_C(0x6)) == UINT64_C(0x6);
}

static int rsd_os_supports_avx512(RsdCpuRegs leaf1) {
    if (!rsd_os_supports_avx(leaf1)) {
        return 0;
    }
    return (rsd_xgetbv(0) & UINT64_C(0xE6)) == UINT64_C(0xE6);
}

int rsd_cpu_has_sse2(void) {
    RsdCpuRegs leaf1;
    if (!rsd_cpuid(1, 0, &leaf1)) {
        return 0;
    }
    return rsd_bit(leaf1.edx, 26);
}

int rsd_cpu_has_sse41(void) {
    RsdCpuRegs leaf1;
    if (!rsd_cpuid(1, 0, &leaf1)) {
        return 0;
    }
    return rsd_bit(leaf1.ecx, 19);
}

int rsd_cpu_has_avx2(void) {
    RsdCpuRegs leaf1;
    RsdCpuRegs leaf7;
    if (!rsd_max_leaf_at_least(7) || !rsd_cpuid(1, 0, &leaf1)) {
        return 0;
    }
    if (!rsd_bit(leaf1.ecx, 28) || !rsd_os_supports_avx(leaf1)) {
        return 0;
    }
    if (!rsd_cpuid(7, 0, &leaf7)) {
        return 0;
    }
    return rsd_bit(leaf7.ebx, 5);
}

int rsd_cpu_has_avx512(void) {
    RsdCpuRegs leaf1;
    RsdCpuRegs leaf7;
    if (!rsd_max_leaf_at_least(7) || !rsd_cpuid(1, 0, &leaf1)) {
        return 0;
    }
    if (!rsd_os_supports_avx512(leaf1)) {
        return 0;
    }
    if (!rsd_cpuid(7, 0, &leaf7)) {
        return 0;
    }
    return rsd_bit(leaf7.ebx, 16) && /* AVX512F */
           rsd_bit(leaf7.ebx, 30) && /* AVX512BW */
           rsd_bit(leaf7.ebx, 31);   /* AVX512VL */
}

#else

int rsd_cpu_has_sse2(void) {
    return 0;
}

int rsd_cpu_has_sse41(void) {
    return 0;
}

int rsd_cpu_has_avx2(void) {
    return 0;
}

int rsd_cpu_has_avx512(void) {
    return 0;
}

#endif

int rsd_cpu_has_neon(void) {
#if defined(__aarch64__) || defined(_M_ARM64)
    return 1;
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
    return 1;
#else
    return 0;
#endif
}

int rsd_cpu_has_wasm_simd128(void) {
#if defined(__wasm__) || defined(__wasm32__) || defined(__wasm64__) || defined(__EMSCRIPTEN__)
    /* A module containing a staged SIMD128 object can only load on a runtime
       with WebAssembly SIMD128 support. There is no in-process CPUID feature
       check for the already-instantiated module. */
    return 1;
#else
    return 0;
#endif
}
