#include "t4/rtinfo.h"

// TODO implement for windows
t4_cpuid_t t4_get_cpuid(const u32 eax) {
    t4_cpuid_t res = {
        .eax = eax,
        .ebx = 0x0,
        .ecx = 0x0,
        .edx = 0x0,
    };

    asm("cpuid\n\t" : "+a"(res.eax), "=b"(res.ebx), "+c"(res.ecx), "=d"(res.edx));

    return res;
}

t4_cpu_features_t t4_get_cpu_features(void) {
    t4_cpu_features_t features = {
        .bmi1 = false,
        .avx2 = false,
        .sse4_2 = false,
    };

    t4_cpuid_t cpuid = t4_get_cpuid(0x7);
    features.bmi1 = (cpuid.ebx & T4_FEATURE_BIT_BMI1) != 0;
    features.avx2 = (cpuid.ebx & T4_FEATURE_BIT_AVX2) != 0;

    cpuid = t4_get_cpuid(0x1);
    features.sse4_2 = (cpuid.ecx & T4_FEATURE_BIT_SSE4_2) != 0;

    return features;
}
