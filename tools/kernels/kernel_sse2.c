/* Staged by configure with SSE2 enabled. */

#include <stddef.h>
#include <stdint.h>

#include <simde/x86/sse2.h>

#include "kernel_common.h"
#include "kernel_api.h"

size_t rsd_count_nonzero_sse2(const uint8_t *x, size_t n) {
    size_t i = 0;
    size_t acc = 0;
    const simde__m128i zero = simde_mm_setzero_si128();

    for (; i + 16 <= n; i += 16) {
        simde__m128i v = simde_mm_loadu_si128((const simde__m128i *)(const void *)(x + i));
        simde__m128i eq = simde_mm_cmpeq_epi8(v, zero);
        uint32_t mask = (uint32_t)simde_mm_movemask_epi8(eq);
        acc += 16u - rsd_popcount32(mask);
    }

    for (; i < n; ++i) {
        acc += x[i] != 0;
    }
    return acc;
}

static void rsd_count_nonzero_sse2_invoke(void *call) {
    RsdCountNonzeroCall *args = (RsdCountNonzeroCall *)call;
    args->result = rsd_count_nonzero_sse2(args->x, args->n);
}

static const RsdKernelDef rsd_sse2_kernels[] = {
    {RSD_OP_COUNT_NONZERO, RSD_SIG_RAW_COUNT, rsd_count_nonzero_sse2_invoke},
    RSD_KERNEL_DEF_END
};

void rsd_register_sse2(RsdDispatchBuilder *builder) {
    rsd_register_kernel_table(builder, rsd_sse2_kernels);
}
