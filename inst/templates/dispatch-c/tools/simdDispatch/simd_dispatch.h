#ifndef SD_SIMD_DISPATCH_H
#define SD_SIMD_DISPATCH_H

#include <stddef.h>
#include <stdint.h>

#include "kernels/kernel_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/* --------------------------------------------------------------------------
 * Error handler
 *
 * The dispatch engine does not depend on R headers. All fatal errors are
 * routed through a pluggable handler. The default handler writes to stderr
 * and calls abort(). R callers should install sd_r_error_handler() (see
 * registration.c) which redirects to Rf_error() so that R's error-handling
 * and longjmp machinery work correctly.
 *
 * The handler receives a fully-formatted, null-terminated message string.
 * It must not return; if it does, the dispatch engine calls abort().
 * -------------------------------------------------------------------------- */

typedef void (*sd_error_handler_fn)(const char *msg);

/* Install a custom error handler. Passing NULL restores the default. */
void sd_set_error_handler(sd_error_handler_fn handler);

/* --------------------------------------------------------------------------
 * Dispatch lifecycle
 * -------------------------------------------------------------------------- */

/* Initialise the dispatch table using auto-selection. No-op if already
 * initialised. Called automatically on first use, but also called eagerly
 * by R_init_* so the selected backend is deterministic at package load. */
void sd_init_dispatch(void);

/* Select a backend by name. Pass "auto" to revert to auto-selection.
 * Raises an error (via the installed handler) for unknown, uncompiled, or
 * CPU-unsupported backends. */
void sd_set_backend(const char *backend);

/* --------------------------------------------------------------------------
 * Dispatch invocation
 * -------------------------------------------------------------------------- */

/* Invoke the resolved kernel for the given operation.
 * - operation:      which operation to dispatch
 * - signature:      expected kernel signature; raises an error on mismatch
 * - call:           pointer to the operation-specific call-frame struct
 * - operation_name: human-readable name used in error messages (may be NULL)
 */
void sd_dispatch_invoke(SdOperation operation, SdKernelSignature signature,
                         void *call, const char *operation_name);

/* --------------------------------------------------------------------------
 * Operation introspection
 *
 * These functions expose the canonical operation catalog so consumers (e.g.
 * the R API layer) do not maintain a separate parallel table.
 * -------------------------------------------------------------------------- */

/* Number of registered operations (equals SD_OP_COUNT). */
size_t sd_operation_count(void);

/* Name string for the i-th operation, or NULL if out of range. */
const char *sd_operation_name(SdOperation operation);

/* Canonical call-frame signature for the i-th operation.
 * Returns SD_SIG_NONE if operation is out of range. */
SdKernelSignature sd_operation_expected_signature(SdOperation operation);

/* Name string for a kernel signature (e.g. "RAW_COUNT"), or NULL for
 * SD_SIG_NONE or out-of-range values. */
const char *sd_signature_name(SdKernelSignature sig);

/* --------------------------------------------------------------------------
 * Backend introspection
 * -------------------------------------------------------------------------- */

/* Returns the requested backend string ("auto" or a named backend). */
const char *sd_requested_backend(void);

/* Returns a summary string for the currently selected backend.
 * Possible values:
 *   - a backend name   (all operations resolved to the same backend)
 *   - "partial:<name>" (explicit backend selected but covers only a subset
 *                       of operations; some operations are unresolved)
 *   - "mixed"          (different operations resolved to different backends;
 *                       only possible under auto-dispatch)
 *   - "unavailable"    (no operation could be resolved)
 *   - "uninitialized"  (dispatch has not been initialised yet)
 */
const char *sd_selected_backend(void);

/* Returns the backend name selected for a specific operation, or NULL if
 * the operation has no resolved kernel. */
const char *sd_operation_selected_backend(SdOperation operation);

/* Returns 1 if backend is in the known backend list. */
int sd_backend_known(const char *backend);

/* Returns 1 if backend was compiled into this build. */
int sd_backend_compiled(const char *backend);

/* Returns 1 if the current CPU/runtime supports backend at runtime. */
int sd_backend_cpu_supported(const char *backend);

/* Returns 1 if backend is both compiled and CPU-supported. */
int sd_backend_available(const char *backend);

/* Returns 1 if backend was compiled with native (non-SIMDe) intrinsics,
 * i.e. the compiler targeted that ISA directly. */
int sd_backend_simde_native(const char *backend);

/* Returns 1 if backend is available AND provides a kernel for operation. */
int sd_backend_operation_available(const char *backend, SdOperation operation);

/* Total number of known backends (compiled or not). */
size_t sd_backend_count(void);

/* Name of the i-th backend, or NULL if i is out of range. */
const char *sd_backend_name(size_t i);

/* A stable string identifying the dispatch implementation strategy. */
const char *sd_dispatch_mode(void);

#ifdef __cplusplus
}
#endif

#endif
