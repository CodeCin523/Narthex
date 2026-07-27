#ifndef NTH_UTILS_ARCH_H
#define NTH_UTILS_ARCH_H

#ifdef __cplusplus
extern "C" {
#endif


#define NTH_ARCH_X86_64 0
#define NTH_ARCH_ARM64 0
#define NTH_ARCH_RISCV64 0
#define NTH_ARCH_UNKNOWN 0

#if defined(__x86_64__) || defined(_M_X64)
    #undef NTH_ARCH_X86_64
    #define NTH_ARCH_X86_64 1
#elif defined(__aarch64__) || defined(_M_ARM64)
    #undef NTH_ARCH_ARM64
    #define NTH_ARCH_ARM64 1
#elif defined(__riscv) && __riscv_xlen == 64
    #undef  NTH_ARCH_RISCV64
    #define NTH_ARCH_RISCV64 1
#else
    #undef NTH_ARCH_UNKNOWN
    #define NTH_ARCH_UNKNOWN 1
#endif


#ifdef __cplusplus
}
#endif

#endif /* NTH_UTILS_ARCH_H */