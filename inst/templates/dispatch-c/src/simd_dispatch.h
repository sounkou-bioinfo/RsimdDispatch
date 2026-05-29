#ifndef RSD_SIMD_DISPATCH_H
#define RSD_SIMD_DISPATCH_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum RsdOperation {
    RSD_OP_COUNT_NONZERO = 0,
    RSD_OP_CONVOLVE1D = 1,
    RSD_OP_COUNT = 2
} RsdOperation;

typedef size_t (*rsd_count_nonzero_fn)(const uint8_t *x, size_t n);
typedef void (*rsd_convolve1d_fn)(const double *a, size_t na, const double *b, size_t nb, double *out);

typedef struct RsdDispatchBuilder RsdDispatchBuilder;

void rsd_register_count_nonzero(RsdDispatchBuilder *builder, rsd_count_nonzero_fn fn);
void rsd_register_convolve1d(RsdDispatchBuilder *builder, rsd_convolve1d_fn fn);

void rsd_init_dispatch(void);
void rsd_set_backend(const char *backend);
const char *rsd_requested_backend(void);
const char *rsd_selected_backend(void);
const char *rsd_operation_selected_backend(const char *operation);
size_t rsd_count_nonzero(const uint8_t *x, size_t n);
void rsd_convolve1d(const double *a, size_t na, const double *b, size_t nb, double *out);
int rsd_backend_known(const char *backend);
int rsd_backend_compiled(const char *backend);
int rsd_backend_cpu_supported(const char *backend);
int rsd_backend_available(const char *backend);
int rsd_backend_operation_available(const char *backend, const char *operation);
size_t rsd_backend_count(void);
const char *rsd_backend_name(size_t i);
size_t rsd_operation_count(void);
const char *rsd_operation_name(size_t i);
const char *rsd_dispatch_mode(void);

#ifdef __cplusplus
}
#endif

#endif
