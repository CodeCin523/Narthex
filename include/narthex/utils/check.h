#ifndef NTH_UTILS_CHECK_H
#define NTH_UTILS_CHECK_H

#include <narthex/utils/build.h>
#include <narthex/utils/compiler.h>

#ifdef __cplusplus
extern "C" {
#endif


#if NTH_DEBUG
#include <stdio.h>

static inline NTH_NORETURN void nth_assert_fail(const char *expr, const char *file, int line, const char *func) {
    fprintf(stderr, "%s:%d: %s: assertion failed: %s\n", file, line, func, expr);
    fflush(stderr);

    NTH_TRAP();
}
#endif


/* Always fatal in debug and release. Use this for internal invariants that must not fail. */
#if NTH_DEBUG
    #define NTH_ASSERT(expr) do { if (!(expr)) nth_assert_fail(#expr, __FILE__, __LINE__, __func__); } while (0)
#else
    #define NTH_ASSERT(expr) do { if (!(expr)) NTH_TRAP(); } while (0)
#endif

/* Returns ret if the expression is false. Use this for runtime failures that the caller must handle. */
#define NTH_CHECK(expr, ret) do { if (!(expr)) return (ret); } while (0)

/* In debug mode, traps if the expression is false. In release mode, does nothing. Use this for debug-only invariants. */
#if NTH_DEBUG
    #define NTH_DASSERT(expr) NTH_ASSERT(expr)
#else
    #define NTH_DASSERT(expr) ((void)sizeof(expr))
#endif

/* In debug mode, returns ret if the expression is false. In release mode, does nothing. Use this to validate arguments. */
#if NTH_DEBUG
    #define NTH_DCHECK(expr, ret) do { if (!(expr)) return (ret); } while (0)
#else
    #define NTH_DCHECK(expr, ret) ((void)sizeof(expr))
#endif

/* In debug mode, traps. In release mode, tells the optimizer that this path does not execute. */
#if NTH_DEBUG
    #define NTH_UNREACHABLE() nth_assert_fail("unreachable", __FILE__, __LINE__, __func__)
#else
    #define NTH_UNREACHABLE() NTH_ASSUME_UNREACHABLE()
#endif


#ifdef __cplusplus
}
#endif

#endif /* NTH_UTILS_CHECK_H */