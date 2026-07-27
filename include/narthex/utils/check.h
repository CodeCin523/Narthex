#ifndef NTH_UTILS_CHECK_H
#define NTH_UTILS_CHECK_H

#ifdef __cplusplus
extern "C" {
#endif

#include <narthex/utils/build.h>
#include <narthex/utils/compiler.h>


/* Always fatal in debug and release. Use this for internal invariants that must not fail. */
#define NTH_ASSERT(expr) do { if (!(expr)) NTH_TRAP(); } while (0)

/* Returns ret if the expression is false. Use this for runtime failures that the caller must handle. */
#define NTH_CHECK(expr, ret) do { if (!(expr)) return (ret); } while (0)

/* In debug mode, traps if the expression is false. In release mode, does nothing. Use this for debug-only invariants. */
#if NTH_DEBUG
#  define NTH_DASSERT(expr) NTH_ASSERT(expr)
#else
#  define NTH_DASSERT(expr) ((void)sizeof(expr))
#endif

/* In debug mode, returns ret if the expression is false. In release mode, does nothing. Use this to validate arguments. */
#if NTH_DEBUG
#  define NTH_DCHECK(expr, ret) do { if (!(expr)) return (ret); } while (0)
#else
#  define NTH_DCHECK(expr, ret) ((void)sizeof(expr))
#endif

/* In debug mode, traps. In release mode, tells the optimizer that this path does not execute. */
#if NTH_DEBUG
#  define NTH_UNREACHABLE() NTH_TRAP()
#else
#  define NTH_UNREACHABLE() NTH_ASSUME_UNREACHABLE()
#endif


#ifdef __cplusplus
}
#endif

#endif /* NTH_UTILS_CHECK_H */
