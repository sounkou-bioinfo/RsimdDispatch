/* Staged by configure with SSE4.1 enabled. */

#include <stddef.h>
#include <stdint.h>

#include <simde/x86/sse4.1.h>

#include "kernel_common.h"
#include "kernel_api.h"

size_t sd_count_nonzero_sse41(const uint8_t *x, size_t n) {
    size_t i = 0;
    size_t acc = 0;
    const simde__m128i zero = simde_mm_setzero_si128();

    for (; i + 16 <= n; i += 16) {
        simde__m128i v = simde_mm_loadu_si128((const simde__m128i *)(const void *)(x + i));
        if (simde_mm_testz_si128(v, v)) {
            continue;
        }
        simde__m128i eq = simde_mm_cmpeq_epi8(v, zero);
        uint32_t mask = (uint32_t)simde_mm_movemask_epi8(eq);
        acc += 16u - sd_popcount32(mask);
    }

    for (; i < n; ++i) {
        acc += x[i] != 0;
    }
    return acc;
}

static void sd_count_nonzero_sse41_invoke(void *call) {
    SdCountNonzeroCall *args = (SdCountNonzeroCall *)call;
    args->result = sd_count_nonzero_sse41(args->x, args->n);
}

static const SdKernelDef sd_sse41_kernels[] = {
    {SD_OP_COUNT_NONZERO, SD_SIG_RAW_COUNT, sd_count_nonzero_sse41_invoke},
    SD_KERNEL_DEF_END
};

void sd_register_sse41(SdDispatchBuilder *builder) {
    sd_register_kernel_table(builder, sd_sse41_kernels);
}
