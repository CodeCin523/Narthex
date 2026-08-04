#ifndef NTH_SRC_POISON_H
#define NTH_SRC_POISON_H

#include <narthex/nth_types.h>

#if defined(__SANITIZE_ADDRESS__)
    #define NTH_HAS_ASAN 1
#elif defined(__has_feature)
    #if __has_feature(address_sanitizer)
        #define NTH_HAS_ASAN 1
    #endif
#endif

#ifndef NTH_HAS_ASAN
    #define NTH_HAS_ASAN 0
#endif

#ifndef NTH_POISON
    #define NTH_POISON 0
#endif

#if NTH_HAS_ASAN
    #include <sanitizer/asan_interface.h>
    #define NTH_ASAN_OFF(p, n) ASAN_POISON_MEMORY_REGION((p), (n))
    #define NTH_ASAN_ON(p, n)  ASAN_UNPOISON_MEMORY_REGION((p), (n))
#else
    #define NTH_ASAN_OFF(p, n) ((void)0)
    #define NTH_ASAN_ON(p, n)  ((void)0)
#endif


#if NTH_POISON || NTH_HAS_ASAN

#include <string.h>

#define NTH_POISON_FRESH 0xCD
#define NTH_POISON_DEAD  0xDD

static inline void nth_poison_dead(void *p, nth_usize n) {
    if (p == NULL || n == 0)
        return;
#if NTH_POISON
    NTH_ASAN_ON(p, n);
    memset(p, NTH_POISON_DEAD, n);
#endif
    NTH_ASAN_OFF(p, n);
}

static inline void nth_poison_live(void *p, nth_usize n) {
    if (p == NULL || n == 0)
        return;
    NTH_ASAN_ON(p, n);
#if NTH_POISON
    memset(p, NTH_POISON_FRESH, n);
#endif
}

static inline void nth_poison_disown(void *p, nth_usize n) {
    if (p == NULL || n == 0)
        return;
    NTH_ASAN_ON(p, n);
}

#else

#define nth_poison_dead(p, n)   ((void)0)
#define nth_poison_live(p, n)   ((void)0)
#define nth_poison_disown(p, n) ((void)0)

#endif

#endif /* NTH_SRC_POISON_H */
