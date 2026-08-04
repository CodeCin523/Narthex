#ifndef NTH_UTILS_COMPILER_H
#define NTH_UTILS_COMPILER_H

#ifdef __cplusplus
extern "C" {
#endif


#define NTH_COMPILER_MSVC    0
#define NTH_COMPILER_CLANG   0
#define NTH_COMPILER_GCC     0
#define NTH_COMPILER_UNKNOWN 0

#if defined(_MSC_VER)
    #undef  NTH_COMPILER_MSVC
    #define NTH_COMPILER_MSVC 1
#elif defined(__clang__)
    #undef  NTH_COMPILER_CLANG
    #define NTH_COMPILER_CLANG 1
#elif defined(__GNUC__)
    #undef  NTH_COMPILER_GCC
    #define NTH_COMPILER_GCC 1
#else
    #undef  NTH_COMPILER_UNKNOWN
    #define NTH_COMPILER_UNKNOWN 1
#endif


/* Halts in the debugger. On unsupported compilers, calls abort(). */
#if NTH_COMPILER_MSVC
    #define NTH_TRAP() __debugbreak()
#elif NTH_COMPILER_CLANG || NTH_COMPILER_GCC
    #define NTH_TRAP() __builtin_trap()
#else
    #include <stdlib.h>
    #define NTH_TRAP() abort()
#endif

/* Forces the compiler to inline the function. The compiler ignores optimizer heuristics. */
#if NTH_COMPILER_MSVC
    #define NTH_FORCEINLINE __forceinline
#elif NTH_COMPILER_CLANG || NTH_COMPILER_GCC
    #define NTH_FORCEINLINE __attribute__((always_inline)) inline
#else
    #define NTH_FORCEINLINE inline
#endif

/* Aligns a declaration. Spelled with attributes so it stays valid in C99 and C++. */
#if NTH_COMPILER_MSVC
    #define NTH_ALIGNAS(n) __declspec(align(n))
#else
    #define NTH_ALIGNAS(n) __attribute__((aligned(n)))
#endif

/* Prevents the compiler from inlining the function. */
#if NTH_COMPILER_MSVC
    #define NTH_NOINLINE __declspec(noinline)
#elif NTH_COMPILER_CLANG || NTH_COMPILER_GCC
    #define NTH_NOINLINE __attribute__((noinline))
#else
    #define NTH_NOINLINE
#endif

/* Gives the compiler a branch prediction hint. */
#if NTH_COMPILER_CLANG || NTH_COMPILER_GCC
    #define NTH_LIKELY(expr)   __builtin_expect(!!(expr), 1)
    #define NTH_UNLIKELY(expr) __builtin_expect(!!(expr), 0)
#else
    #define NTH_LIKELY(expr)   (expr)
    #define NTH_UNLIKELY(expr) (expr)
#endif

/* Tells the optimizer that this code path does not execute. */
#if NTH_COMPILER_MSVC
    #define NTH_ASSUME_UNREACHABLE() __assume(0)
#elif NTH_COMPILER_CLANG || NTH_COMPILER_GCC
    #define NTH_ASSUME_UNREACHABLE() __builtin_unreachable()
#else
    #define NTH_ASSUME_UNREACHABLE() ((void)0)
#endif


#ifdef __cplusplus
}
#endif

#endif /* NTH_UTILS_COMPILER_H */
