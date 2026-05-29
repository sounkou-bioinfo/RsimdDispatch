#ifndef RSD_SIMD_DISPATCH_H
#define RSD_SIMD_DISPATCH_H

#include <stddef.h>
#include <stdint.h>

#include "kernel_api.h"

#ifdef __cplusplus
extern "C" {
#endif

void rsd_init_dispatch(void);
void rsd_set_backend(const char *backend);
const char *rsd_requested_backend(void);
const char *rsd_selected_backend(void);
const char *rsd_operation_selected_backend(RsdOperation operation);
void rsd_dispatch_invoke(RsdOperation operation, RsdKernelSignature signature, void *call, const char *operation_name);
int rsd_backend_known(const char *backend);
int rsd_backend_compiled(const char *backend);
int rsd_backend_cpu_supported(const char *backend);
int rsd_backend_available(const char *backend);
int rsd_backend_operation_available(const char *backend, RsdOperation operation);
size_t rsd_backend_count(void);
const char *rsd_backend_name(size_t i);
const char *rsd_dispatch_mode(void);

#ifdef __cplusplus
}
#endif

#endif
