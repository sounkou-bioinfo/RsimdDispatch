#ifndef RSD_SIMD_DISPATCH_H
#define RSD_SIMD_DISPATCH_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef size_t (*rsd_count_nonzero_fn)(const uint8_t *x, size_t n);

void rsd_init_dispatch(void);
void rsd_set_backend(const char *backend);

size_t rsd_count_nonzero(const uint8_t *x, size_t n);

int rsd_backend_known(const char *backend);
int rsd_backend_compiled(const char *backend);
int rsd_backend_cpu_supported(const char *backend);
int rsd_backend_available(const char *backend);

const char *rsd_requested_backend(void);
const char *rsd_selected_backend(void);
const char *rsd_compiled_backends_csv(void);
const char *rsd_cpu_supported_backends_csv(void);
const char *rsd_available_backends_csv(void);
const char *rsd_dispatch_mode(void);

#ifdef __cplusplus
}
#endif

#endif
