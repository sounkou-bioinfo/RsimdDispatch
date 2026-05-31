/* --------------------------------------------------------------------------
 * Backend availability detection.
 * SD_HAVE_* may be pre-defined by the R build system via a generated
 * config.h (passed through -DSD_HAVE_CONFIG_H).  When building standalone
 * the macros fall back to compiler-defined ISA macros; the standalone
 * Makefile compiles this file with all available -m flags so they are set.
 * -------------------------------------------------------------------------- */
#if defined(SD_HAVE_CONFIG_H)
#  include "config.h"
#endif
#ifndef SD_HAVE_SSE2
#  ifdef __SSE2__
#    define SD_HAVE_SSE2 1
#  else
#    define SD_HAVE_SSE2 0
#  endif
#endif
#ifndef SD_HAVE_SSE41
#  ifdef __SSE4_1__
#    define SD_HAVE_SSE41 1
#  else
#    define SD_HAVE_SSE41 0
#  endif
#endif
#ifndef SD_HAVE_AVX2
#  ifdef __AVX2__
#    define SD_HAVE_AVX2 1
#  else
#    define SD_HAVE_AVX2 0
#  endif
#endif
#ifndef SD_HAVE_AVX512
#  if defined(__AVX512F__) && defined(__AVX512BW__) && defined(__AVX512VL__)
#    define SD_HAVE_AVX512 1
#  else
#    define SD_HAVE_AVX512 0
#  endif
#endif
#ifndef SD_HAVE_NEON
#  if defined(__ARM_NEON) || defined(__ARM_NEON__)
#    define SD_HAVE_NEON 1
#  else
#    define SD_HAVE_NEON 0
#  endif
#endif
#ifndef SD_HAVE_WASM_SIMD128
#  ifdef __wasm_simd128__
#    define SD_HAVE_WASM_SIMD128 1
#  else
#    define SD_HAVE_WASM_SIMD128 0
#  endif
#endif

/* SD_SIMDE_NATIVE_* — 1 when the backend was compiled with native (non-SIMDe)
 * intrinsics, i.e. the compiler targeted that ISA directly.
 * Set by configure via config.h; default to 0 for standalone builds. */
#ifndef SD_SIMDE_NATIVE_SSE2
#  define SD_SIMDE_NATIVE_SSE2 0
#endif
#ifndef SD_SIMDE_NATIVE_SSE41
#  define SD_SIMDE_NATIVE_SSE41 0
#endif
#ifndef SD_SIMDE_NATIVE_AVX2
#  define SD_SIMDE_NATIVE_AVX2 0
#endif
#ifndef SD_SIMDE_NATIVE_AVX512
#  define SD_SIMDE_NATIVE_AVX512 0
#endif
#ifndef SD_SIMDE_NATIVE_NEON
#  define SD_SIMDE_NATIVE_NEON 0
#endif
#ifndef SD_SIMDE_NATIVE_WASM_SIMD128
#  define SD_SIMDE_NATIVE_WASM_SIMD128 0
#endif

#include "simd_dispatch.h"
#include "cpu_features.h"

#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --------------------------------------------------------------------------
 * Pluggable error handler
 * The default writes to stderr and aborts, which is safe in any embedding
 * context.  R callers install sd_r_error_handler() (via registration.c)
 * which redirects to Rf_error() and therefore longjmps as R expects.
 * -------------------------------------------------------------------------- */

static void sd_default_error_handler(const char *msg) {
    fprintf(stderr, "RsimdDispatch error: %s\n", msg);
    abort();
}

static sd_error_handler_fn sd_error_handler = sd_default_error_handler;

void sd_set_error_handler(sd_error_handler_fn handler) {
    sd_error_handler = handler != NULL ? handler : sd_default_error_handler;
}

#define SD_ERROR_BUF_SIZE 512

static void sd_raise_error(const char *fmt, ...) {
    char buf[SD_ERROR_BUF_SIZE];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    sd_error_handler(buf);
    /* Safety net: if the handler returns (it must not), abort to prevent UB. */
    abort();
}

/* --------------------------------------------------------------------------
 * Operation catalog — generated from operations.def.
 *
 * This is the single source of truth for operation names and their canonical
 * call-frame signatures.  The R API layer and the registration engine both
 * query this catalog rather than maintaining parallel tables.
 * -------------------------------------------------------------------------- */

typedef struct {
    SdOperation operation;
    SdKernelSignature signature;
    const char *name;
} SdOperationCatalogEntry;

static const SdOperationCatalogEntry sd_operation_catalog[] = {
#define SD_OPERATION(ID, name, sig, call_type) \
    {SD_OP_##ID, SD_SIG_##sig, #name},
#include "kernels/operations.def"
#undef SD_OPERATION
};

size_t sd_operation_count(void) {
    return sizeof(sd_operation_catalog) / sizeof(sd_operation_catalog[0]);
}

const char *sd_operation_name(SdOperation operation) {
    if ((size_t)operation >= sd_operation_count()) {
        return NULL;
    }
    return sd_operation_catalog[operation].name;
}

SdKernelSignature sd_operation_expected_signature(SdOperation operation) {
    if ((size_t)operation >= sd_operation_count()) {
        return SD_SIG_NONE;
    }
    return sd_operation_catalog[operation].signature;
}

/* --------------------------------------------------------------------------
 * Backend priority constants
 * Lower value = higher priority: the backend with the smallest priority number
 * wins the slot for a given operation during auto-dispatch.
 * -------------------------------------------------------------------------- */

#define SD_PRIORITY_AVX512       10u
#define SD_PRIORITY_AVX2         20u
#define SD_PRIORITY_SSE41        30u
#define SD_PRIORITY_SSE2         40u
#define SD_PRIORITY_NEON         50u
#define SD_PRIORITY_WASM_SIMD128 60u
#define SD_PRIORITY_SCALAR       70u

static int sd_cpu_has_scalar_backend(void) {
    return 1;
}

typedef void (*SdBackendRegisterFn)(SdDispatchBuilder *builder);

typedef struct SdBackendEntry {
    const char *name;
    int compiled;
    int simde_native;
    int (*supported)(void);
    SdBackendRegisterFn register_backend;
    unsigned int priority;
} SdBackendEntry;

typedef struct SdResolvedSlot {
    sd_kernel_invoke_fn invoke;
    SdKernelSignature signature;
    const char *backend;
} SdResolvedSlot;

struct SdDispatchBuilder {
    SdResolvedSlot *slots;
    const SdBackendEntry *backend;
    unsigned int best_priority[SD_OP_COUNT];
};

/* --------------------------------------------------------------------------
 * Backend registration function declarations.
 *
 * For compiled backends, declare as extern (defined in kernel_*.c).
 * For uncompiled backends, define a no-op static stub so that the
 * backends.def X-macro can reference sd_register_##name uniformly.
 * The stubs are never called: sd_backend_entry_available() gates on the
 * compiled flag before any call to register_backend.
 * -------------------------------------------------------------------------- */

extern void sd_register_scalar(SdDispatchBuilder *builder);

#if SD_HAVE_SSE2
extern void sd_register_sse2(SdDispatchBuilder *builder);
#else
static void sd_register_sse2(SdDispatchBuilder *b) { (void)b; }
#endif

#if SD_HAVE_SSE41
extern void sd_register_sse41(SdDispatchBuilder *builder);
#else
static void sd_register_sse41(SdDispatchBuilder *b) { (void)b; }
#endif

#if SD_HAVE_AVX2
extern void sd_register_avx2(SdDispatchBuilder *builder);
#else
static void sd_register_avx2(SdDispatchBuilder *b) { (void)b; }
#endif

#if SD_HAVE_AVX512
extern void sd_register_avx512(SdDispatchBuilder *builder);
#else
static void sd_register_avx512(SdDispatchBuilder *b) { (void)b; }
#endif

#if SD_HAVE_NEON
extern void sd_register_neon(SdDispatchBuilder *builder);
#else
static void sd_register_neon(SdDispatchBuilder *b) { (void)b; }
#endif

#if SD_HAVE_WASM_SIMD128
extern void sd_register_wasm_simd128(SdDispatchBuilder *builder);
#else
static void sd_register_wasm_simd128(SdDispatchBuilder *b) { (void)b; }
#endif

/* --------------------------------------------------------------------------
 * Backend entry table — generated from backends.def.
 *
 * All sd_register_* functions are declared above so the X-macro can
 * reference them uniformly.  Uncompiled entries carry their no-op stub but
 * are never invoked because sd_backend_entry_available() checks compiled.
 * -------------------------------------------------------------------------- */

static const SdBackendEntry sd_backend_entries[] = {
#define SD_BACKEND(ID, name, compiled, supported_fn, priority, simde_native_flag) \
    {#name, (int)((compiled) != 0), (int)((simde_native_flag) != 0), supported_fn, sd_register_##name, priority},
#include "backends.def"
#undef SD_BACKEND
};

/* --------------------------------------------------------------------------
 * Dispatch state globals
 *
 * Thread-safety contract: these globals are written only during
 * initialisation (sd_init_dispatch / sd_set_backend) and read during
 * dispatch (sd_dispatch_invoke).  The design relies on the caller
 * completing initialisation before any concurrent dispatch calls are
 * issued.  In the R embedding this is guaranteed because sd_init_dispatch
 * is called from the main R thread before any parallel work, and R's own
 * global interpreter lock prevents concurrent R-level calls.  Standalone
 * embeddings must provide the same serialisation guarantee externally if
 * sd_set_backend may be called concurrently with sd_dispatch_invoke.
 * -------------------------------------------------------------------------- */
static SdResolvedSlot sd_active_slots[SD_OP_COUNT] = {{0}};
static int sd_dispatch_initialized = 0;
static char sd_requested_backend_buf[32] = "auto";
static char sd_selected_backend_buf[32] = "uninitialized";

size_t sd_backend_count(void) {
    return sizeof(sd_backend_entries) / sizeof(sd_backend_entries[0]);
}

const char *sd_backend_name(size_t i) {
    if (i >= sd_backend_count()) {
        return NULL;
    }
    return sd_backend_entries[i].name;
}

static int sd_operation_valid(SdOperation operation) {
    return (int)operation >= 0 && (size_t)operation < sd_operation_count();
}

static const SdBackendEntry *sd_find_backend(const char *backend) {
    if (backend == NULL) {
        return NULL;
    }
    for (size_t i = 0; i < sd_backend_count(); ++i) {
        if (strcmp(backend, sd_backend_entries[i].name) == 0) {
            return &sd_backend_entries[i];
        }
    }
    return NULL;
}

static int sd_backend_entry_cpu_supported(const SdBackendEntry *entry) {
    return entry != NULL && entry->supported != NULL && entry->supported() != 0;
}

static int sd_backend_entry_available(const SdBackendEntry *entry) {
    return entry != NULL && entry->compiled != 0 && sd_backend_entry_cpu_supported(entry);
}

int sd_backend_known(const char *backend) {
    return sd_find_backend(backend) != NULL;
}

int sd_backend_compiled(const char *backend) {
    const SdBackendEntry *entry = sd_find_backend(backend);
    return entry != NULL && entry->compiled != 0;
}

int sd_backend_cpu_supported(const char *backend) {
    return sd_backend_entry_cpu_supported(sd_find_backend(backend));
}

int sd_backend_available(const char *backend) {
    return sd_backend_entry_available(sd_find_backend(backend));
}

int sd_backend_simde_native(const char *backend) {
    const SdBackendEntry *e = sd_find_backend(backend);
    return e != NULL ? e->simde_native : 0;
}

static void sd_builder_init(SdDispatchBuilder *builder, SdResolvedSlot *slots, const SdBackendEntry *backend) {
    builder->slots = slots;
    builder->backend = backend;
    for (size_t i = 0; i < SD_OP_COUNT; ++i) {
        builder->best_priority[i] = UINT_MAX;
    }
}

static int sd_builder_should_update(SdDispatchBuilder *builder, SdOperation operation, const SdKernelDef *kernel) {
    if (builder == NULL || builder->slots == NULL || builder->backend == NULL || kernel == NULL) {
        return 0;
    }
    if (!sd_operation_valid(operation) || kernel->invoke == NULL || kernel->signature == SD_SIG_NONE) {
        return 0;
    }
    return builder->backend->priority < builder->best_priority[operation];
}

void sd_register_kernel_table(SdDispatchBuilder *builder, const SdKernelDef *kernels) {
    if (builder == NULL || kernels == NULL) {
        return;
    }

    /* First pass: validate ALL rows unconditionally.
     * A lower-priority backend with a wrong signature must be caught even if it
     * would never win the priority race.  We check every row before installing
     * any, so that a mis-wired backend always raises an error at registration
     * time regardless of dispatch priority. */
    for (size_t i = 0; kernels[i].invoke != NULL; ++i) {
        SdOperation operation = kernels[i].operation;
        if (!sd_operation_valid(operation)) {
            sd_raise_error(
                "backend '%s' registered unknown operation id %d",
                builder->backend != NULL ? builder->backend->name : "<unknown>",
                (int)operation
            );
        }
        if (kernels[i].signature == SD_SIG_NONE) {
            sd_raise_error(
                "backend '%s' registered operation '%s' with SD_SIG_NONE signature",
                builder->backend != NULL ? builder->backend->name : "<unknown>",
                sd_operation_name(operation) != NULL ? sd_operation_name(operation) : "<unknown>"
            );
        }
        if (kernels[i].signature != sd_operation_expected_signature(operation)) {
            sd_raise_error(
                "backend '%s' registered operation '%s' with wrong signature "
                "(expected %d, got %d)",
                builder->backend != NULL ? builder->backend->name : "<unknown>",
                sd_operation_name(operation) != NULL ? sd_operation_name(operation) : "<unknown>",
                (int)sd_operation_expected_signature(operation),
                (int)kernels[i].signature
            );
        }
    }

    /* Second pass: install rows based on priority. */
    for (size_t i = 0; kernels[i].invoke != NULL; ++i) {
        SdOperation operation = kernels[i].operation;
        if (!sd_builder_should_update(builder, operation, &kernels[i])) {
            continue;
        }
        builder->slots[operation].invoke = kernels[i].invoke;
        builder->slots[operation].signature = kernels[i].signature;
        builder->slots[operation].backend = builder->backend->name;
        builder->best_priority[operation] = builder->backend->priority;
    }
}

static int sd_resolved_slots_has_operation(const SdResolvedSlot *slots, SdOperation operation) {
    if (slots == NULL || !sd_operation_valid(operation)) {
        return 0;
    }
    return slots[operation].invoke != NULL;
}

static const char *sd_resolved_slots_backend_for_operation(const SdResolvedSlot *slots, SdOperation operation) {
    if (slots == NULL || !sd_operation_valid(operation)) {
        return NULL;
    }
    return slots[operation].backend;
}

static void sd_register_available_backend(SdDispatchBuilder *builder, const SdBackendEntry *entry) {
    if (!sd_backend_entry_available(entry) || entry->register_backend == NULL) {
        return;
    }
    builder->backend = entry;
    entry->register_backend(builder);
}

static void sd_build_auto_slots(SdResolvedSlot *slots) {
    SdDispatchBuilder builder;
    memset(slots, 0, sizeof(SdResolvedSlot) * SD_OP_COUNT);
    sd_builder_init(&builder, slots, NULL);
    for (size_t i = 0; i < sd_backend_count(); ++i) {
        sd_register_available_backend(&builder, &sd_backend_entries[i]);
    }
}

static void sd_build_backend_slots(SdResolvedSlot *slots, const SdBackendEntry *entry) {
    SdDispatchBuilder builder;
    memset(slots, 0, sizeof(SdResolvedSlot) * SD_OP_COUNT);
    sd_builder_init(&builder, slots, entry);
    if (entry != NULL && entry->register_backend != NULL) {
        entry->register_backend(&builder);
    }
}

int sd_backend_operation_available(const char *backend, SdOperation operation) {
    const SdBackendEntry *entry = sd_find_backend(backend);
    SdResolvedSlot slots[SD_OP_COUNT];
    if (entry == NULL || !sd_operation_valid(operation)) {
        return 0;
    }
    if (!sd_backend_entry_available(entry)) {
        return 0;
    }
    sd_build_backend_slots(slots, entry);
    return sd_resolved_slots_has_operation(slots, operation);
}

static void sd_update_selected_backend_summary(void) {
    const char *summary = NULL;
    int mixed = 0;

    for (size_t i = 0; i < SD_OP_COUNT; ++i) {
        const char *backend = sd_resolved_slots_backend_for_operation(sd_active_slots, (SdOperation)i);
        if (backend == NULL) {
            continue;
        }
        if (summary == NULL) {
            summary = backend;
        } else if (strcmp(summary, backend) != 0) {
            mixed = 1;
            break;
        }
    }

    if (mixed) {
        summary = "mixed";
    } else if (summary == NULL) {
        summary = "unavailable";
    }
    snprintf(sd_selected_backend_buf, sizeof(sd_selected_backend_buf), "%s", summary);
}

static void sd_resolve_auto(void) {
    sd_build_auto_slots(sd_active_slots);
    sd_update_selected_backend_summary();
    sd_dispatch_initialized = 1;
}

static void sd_resolve_backend(const SdBackendEntry *entry) {
    sd_build_backend_slots(sd_active_slots, entry);
    /* Guard: explicit backend selection must provide at least one operation. */
    size_t resolved = 0;
    for (size_t i = 0; i < (size_t)SD_OP_COUNT; ++i) {
        resolved += (sd_active_slots[i].invoke != NULL) ? 1u : 0u;
    }
    if (resolved == 0) {
        sd_raise_error("backend '%s' does not provide any registered operation",
                       entry->name);
    }
    /* Write "partial:<name>" when the backend covers only a subset of
     * operations; write the plain name when all operations are resolved. */
    if (resolved < (size_t)SD_OP_COUNT) {
        snprintf(sd_selected_backend_buf, sizeof(sd_selected_backend_buf),
                 "partial:%s", entry->name);
    } else {
        snprintf(sd_selected_backend_buf, sizeof(sd_selected_backend_buf),
                 "%s", entry->name);
    }
    sd_dispatch_initialized = 1;
}

static void sd_error_unavailable_operation(const char *operation_name) {
    const char *name = operation_name != NULL ? operation_name : "<unknown>";
    if (strcmp(sd_requested_backend_buf, "auto") == 0) {
        sd_raise_error("operation '%s' is not available for any compiled and CPU-supported backend", name);
    }
    sd_raise_error("operation '%s' is not available for selected backend '%s'", name, sd_requested_backend_buf);
}

void sd_init_dispatch(void) {
    if (!sd_dispatch_initialized) {
        snprintf(sd_requested_backend_buf, sizeof(sd_requested_backend_buf), "%s", "auto");
        sd_resolve_auto();
    }
}

void sd_set_backend(const char *backend) {
    const SdBackendEntry *entry;

    if (backend == NULL || backend[0] == '\0') {
        sd_raise_error("backend must be a non-empty string");
    }
    if (strcmp(backend, "auto") == 0) {
        snprintf(sd_requested_backend_buf, sizeof(sd_requested_backend_buf), "%s", "auto");
        sd_resolve_auto();
        return;
    }

    entry = sd_find_backend(backend);
    if (entry == NULL) {
        sd_raise_error("unknown SIMD backend '%s'", backend);
    }
    if (entry->compiled == 0) {
        sd_raise_error("SIMD backend '%s' was not compiled into this build", backend);
    }
    if (!sd_backend_entry_cpu_supported(entry)) {
        sd_raise_error("SIMD backend '%s' is not supported on this CPU/runtime", backend);
    }

    snprintf(sd_requested_backend_buf, sizeof(sd_requested_backend_buf), "%s", backend);
    sd_resolve_backend(entry);
}

void sd_dispatch_invoke(SdOperation operation, SdKernelSignature signature, void *call, const char *operation_name) {
    if (!sd_operation_valid(operation)) {
        sd_raise_error("unknown operation '%s'", operation_name != NULL ? operation_name : "<unknown>");
    }
    if (call == NULL) {
        sd_raise_error("NULL call frame for operation '%s'",
                       operation_name != NULL ? operation_name : "<unknown>");
    }
    if (!sd_dispatch_initialized) {
        sd_init_dispatch();
    }
    if (!sd_resolved_slots_has_operation(sd_active_slots, operation)) {
        sd_error_unavailable_operation(operation_name);
    }
    if (sd_active_slots[operation].signature != signature) {
        sd_raise_error("internal dispatch signature mismatch for operation '%s'", operation_name != NULL ? operation_name : "<unknown>");
    }
    sd_active_slots[operation].invoke(call);
}

const char *sd_requested_backend(void) {
    return sd_requested_backend_buf;
}

const char *sd_selected_backend(void) {
    if (!sd_dispatch_initialized) {
        sd_init_dispatch();
    }
    return sd_selected_backend_buf;
}

const char *sd_operation_selected_backend(SdOperation operation) {
    if (!sd_dispatch_initialized) {
        sd_init_dispatch();
    }
    return sd_resolved_slots_backend_for_operation(sd_active_slots, operation);
}

const char *sd_dispatch_mode(void) {
    return "single-shlib-resolved-ops-table";
}
