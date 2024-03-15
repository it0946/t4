#ifndef T4_RTINFO_H_
#define T4_RTINFO_H_

#include "t4/common.h"

#if __x86_64__

typedef struct t4_cpuid {
    u32 eax;
    u32 ebx;
    u32 ecx;
    u32 edx;
} t4_cpuid_t;

// Technically I should be calling cpuid with eax+ecx, but there is no case in which I need ecx, so until I find one
// this will be eax only.
extern t4_cpuid_t t4_get_cpuid(u32 eax);

/**
 * These values are taken from:
 *  - Intel® 64 and IA-32 Architectures Software Developer’s Manual Volume 2A: Instruction Set Reference, A-L; page 327
 *      - https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html
 */
enum t4_feature_bit {
    /* in: EAX(0x7), out: EBX */
    T4_FEATURE_BIT_BMI1     = 1 << 3,

    /* in: EAX(0x7), out: EBX */
    T4_FEATURE_BIT_AVX2     = 1 << 5,

    /* in: EAX(0x1), out: ECX */
    T4_FEATURE_BIT_SSE4_2   = 1 << 20,
};

#endif /* __x86_64__ */

typedef struct t4_cpu_features  {
#if __x86_64__

    u32 bmi1 : 1;
    u32 avx2 : 1;
    u32 sse4_2 : 1;

#else
#warning "Only x86_64 intrinsics are used currently."
#endif

} t4_cpu_features_t;

extern t4_cpu_features_t t4_get_cpu_features(void);

#endif /* T4_RTINFO_H_ */
