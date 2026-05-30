#include "cpu_features.h"

#include <stdint.h>

#if defined(__EMSCRIPTEN__)
#define SD_TARGET_OS "emscripten"
#elif defined(_WIN32)
#define SD_TARGET_OS "windows"
#elif defined(__APPLE__)
#define SD_TARGET_OS "darwin"
#elif defined(__linux__)
#define SD_TARGET_OS "linux"
#else
#define SD_TARGET_OS "unknown"
#endif

#if defined(__x86_64__) || defined(_M_X64)
#define SD_TARGET_ARCH "x86_64"
#elif defined(__i386__) || defined(_M_IX86)
#define SD_TARGET_ARCH "x86"
#elif defined(__aarch64__) || defined(_M_ARM64)
#define SD_TARGET_ARCH "aarch64"
#elif defined(__arm__) || defined(_M_ARM)
#define SD_TARGET_ARCH "arm"
#elif defined(__wasm32__)
#define SD_TARGET_ARCH "wasm32"
#elif defined(__wasm64__)
#define SD_TARGET_ARCH "wasm64"
#else
#define SD_TARGET_ARCH "unknown"
#endif

const char *sd_target_arch(void) {
    return SD_TARGET_ARCH;
}

const char *sd_target_os(void) {
    return SD_TARGET_OS;
}

#if defined(__x86_64__) || defined(__i386__) || defined(_M_X64) || defined(_M_IX86)
#define SD_X86 1
#else
#define SD_X86 0
#endif

#if SD_X86

typedef struct SdCpuRegs {
    uint32_t eax;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
} SdCpuRegs;

#if defined(_MSC_VER)
#include <intrin.h>
static int sd_cpuid(uint32_t leaf, uint32_t subleaf, SdCpuRegs *out) {
    int regs[4];
    __cpuidex(regs, (int)leaf, (int)subleaf);
    out->eax = (uint32_t)regs[0];
    out->ebx = (uint32_t)regs[1];
    out->ecx = (uint32_t)regs[2];
    out->edx = (uint32_t)regs[3];
    return 1;
}

static uint64_t sd_xgetbv(uint32_t index) {
    return (uint64_t)_xgetbv(index);
}
#elif defined(__GNUC__) || defined(__clang__)
#if defined(__has_include)
#if __has_include(<cpuid.h>)
#define SD_HAVE_CPUID_H 1
#endif
#endif
#ifndef SD_HAVE_CPUID_H
#define SD_HAVE_CPUID_H 0
#endif
#if SD_HAVE_CPUID_H
#include <cpuid.h>
static int sd_cpuid(uint32_t leaf, uint32_t subleaf, SdCpuRegs *out) {
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
static int sd_cpuid(uint32_t leaf, uint32_t subleaf, SdCpuRegs *out) {
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

static uint64_t sd_xgetbv(uint32_t index) {
    uint32_t eax = 0;
    uint32_t edx = 0;
    __asm__ volatile("xgetbv" : "=a"(eax), "=d"(edx) : "c"(index));
    return ((uint64_t)edx << 32) | eax;
}
#else
static int sd_cpuid(uint32_t leaf, uint32_t subleaf, SdCpuRegs *out) {
    (void)leaf;
    (void)subleaf;
    out->eax = out->ebx = out->ecx = out->edx = 0;
    return 0;
}

static uint64_t sd_xgetbv(uint32_t index) {
    (void)index;
    return 0;
}
#endif

static int sd_bit(uint32_t value, unsigned int bit) {
    return (value & (UINT32_C(1) << bit)) != 0;
}

static int sd_max_leaf_at_least(uint32_t leaf) {
    SdCpuRegs regs;
    if (!sd_cpuid(0, 0, &regs)) {
        return 0;
    }
    return regs.eax >= leaf;
}

static int sd_os_supports_avx(SdCpuRegs leaf1) {
    if (!sd_bit(leaf1.ecx, 26) || !sd_bit(leaf1.ecx, 27)) {
        return 0;
    }
    return (sd_xgetbv(0) & UINT64_C(0x6)) == UINT64_C(0x6);
}

static int sd_os_supports_avx512(SdCpuRegs leaf1) {
    if (!sd_os_supports_avx(leaf1)) {
        return 0;
    }
    return (sd_xgetbv(0) & UINT64_C(0xE6)) == UINT64_C(0xE6);
}

int sd_cpu_has_sse2(void) {
    SdCpuRegs leaf1;
    if (!sd_cpuid(1, 0, &leaf1)) {
        return 0;
    }
    return sd_bit(leaf1.edx, 26);
}

int sd_cpu_has_sse41(void) {
    SdCpuRegs leaf1;
    if (!sd_cpuid(1, 0, &leaf1)) {
        return 0;
    }
    return sd_bit(leaf1.ecx, 19);
}

int sd_cpu_has_avx2(void) {
    SdCpuRegs leaf1;
    SdCpuRegs leaf7;
    if (!sd_max_leaf_at_least(7) || !sd_cpuid(1, 0, &leaf1)) {
        return 0;
    }
    if (!sd_bit(leaf1.ecx, 28) || !sd_os_supports_avx(leaf1)) {
        return 0;
    }
    if (!sd_cpuid(7, 0, &leaf7)) {
        return 0;
    }
    return sd_bit(leaf7.ebx, 5);
}

int sd_cpu_has_avx512(void) {
    SdCpuRegs leaf1;
    SdCpuRegs leaf7;
    if (!sd_max_leaf_at_least(7) || !sd_cpuid(1, 0, &leaf1)) {
        return 0;
    }
    if (!sd_os_supports_avx512(leaf1)) {
        return 0;
    }
    if (!sd_cpuid(7, 0, &leaf7)) {
        return 0;
    }
    return sd_bit(leaf7.ebx, 16) && /* AVX512F */
           sd_bit(leaf7.ebx, 30) && /* AVX512BW */
           sd_bit(leaf7.ebx, 31);   /* AVX512VL */
}

#else

int sd_cpu_has_sse2(void) {
    return 0;
}

int sd_cpu_has_sse41(void) {
    return 0;
}

int sd_cpu_has_avx2(void) {
    return 0;
}

int sd_cpu_has_avx512(void) {
    return 0;
}

#endif

int sd_cpu_has_neon(void) {
#if defined(__aarch64__) || defined(_M_ARM64)
    return 1;
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
    return 1;
#else
    return 0;
#endif
}

int sd_cpu_has_wasm_simd128(void) {
#if defined(__wasm__) || defined(__wasm32__) || defined(__wasm64__) || defined(__EMSCRIPTEN__)
    /* A module containing a staged SIMD128 object can only load on a runtime
       with WebAssembly SIMD128 support. There is no in-process CPUID feature
       check for the already-instantiated module. */
    return 1;
#else
    return 0;
#endif
}
