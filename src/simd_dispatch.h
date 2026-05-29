#ifndef RSD_SIMD_DISPATCH_H
#define RSD_SIMD_DISPATCH_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RSD_DISPATCH_OPS(X, data) \
    X(data, count_nonzero, size_t, (const uint8_t *x, size_t n), (x, n), return) \
    X(data, convolve1d, void, (const double *a, size_t na, const double *b, size_t nb, double *out), (a, na, b, nb, out), )

#define RSD_TYPEDEF_OP(data, name, ret, args, call_args, return_stmt) typedef ret (*rsd_##name##_fn) args;
RSD_DISPATCH_OPS(RSD_TYPEDEF_OP, _)
#undef RSD_TYPEDEF_OP

typedef struct RsdOps {
#define RSD_FIELD_OP(data, name, ret, args, call_args, return_stmt) rsd_##name##_fn name;
    RSD_DISPATCH_OPS(RSD_FIELD_OP, _)
#undef RSD_FIELD_OP
} RsdOps;

void rsd_init_dispatch(void);
void rsd_set_backend(const char *backend);
const char *rsd_requested_backend(void);
const char *rsd_selected_backend(void);
#define RSD_DECLARE_DISPATCH_WRAPPER(data, name, ret, args, call_args, return_stmt) ret rsd_##name args;
RSD_DISPATCH_OPS(RSD_DECLARE_DISPATCH_WRAPPER, _)
#undef RSD_DECLARE_DISPATCH_WRAPPER
int rsd_backend_known(const char *backend);
int rsd_backend_compiled(const char *backend);
int rsd_backend_cpu_supported(const char *backend);
int rsd_backend_available(const char *backend);
size_t rsd_backend_count(void);
const char *rsd_backend_name(size_t i);
const char *rsd_dispatch_mode(void);

#ifdef __cplusplus
}
#endif

#endif
