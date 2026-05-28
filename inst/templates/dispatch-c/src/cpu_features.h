#ifndef RSD_CPU_FEATURES_H
#define RSD_CPU_FEATURES_H

#ifdef __cplusplus
extern "C" {
#endif

int rsd_cpu_has_sse2(void);
int rsd_cpu_has_sse41(void);
int rsd_cpu_has_avx2(void);
int rsd_cpu_has_avx512(void);
int rsd_cpu_has_neon(void);

const char *rsd_target_arch(void);
const char *rsd_target_os(void);

#ifdef __cplusplus
}
#endif

#endif
