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


/* arm64 is 128 for Apple Silicon. Drop that support and it can go back to 64. */
#if defined(__GCC_DESTRUCTIVE_SIZE)
    #define NTH_CACHELINE __GCC_DESTRUCTIVE_SIZE
#elif NTH_ARCH_ARM64
    #define NTH_CACHELINE 128
#else
    #define NTH_CACHELINE 64
#endif

#define NTH_CACHE_ISOLATE(type, name) \
    union { type name; char name##_line[NTH_CACHELINE]; }


#ifdef __cplusplus
}
#endif

#endif /* NTH_UTILS_ARCH_H */