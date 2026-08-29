#ifndef NTH_MEM_ALLOCATOR_H
#define NTH_MEM_ALLOCATOR_H

#include <narthex/nth_types.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef struct NthAllocator {
    const void *ctx;

    void *(*alloc)(const void *ctx, nth_usize size, nth_usize align);
    void  (*free)(const void *ctx, void *ptr);
    void *(*realloc)(const void *ctx, void *ptr, nth_usize size, nth_usize align);
    void (*clear)(const void *ctx);
} NthAllocator;


static inline void *nth_alloc(NthAllocator *a, nth_usize size, nth_usize align) {
    return a->alloc(a->ctx, size, align);
}
static inline void nth_free(NthAllocator *a, void *ptr) {
    a->free(a->ctx, ptr);
}
static inline void *nth_realloc(NthAllocator *a, void *ptr, nth_usize new_size, nth_usize align) {
    return a->realloc(a->ctx, ptr, new_size, align);
}
static inline void nth_clear(NthAllocator *a) {
    a->clear(a->ctx);
}

#define NTH_ALLOC_T(a, T) \
    ((T *)nth_alloc((a), sizeof(T), _Alignof(T)))
#define NTH_ALLOC_ARRAY(a, T, n) \
    ((T *)nth_alloc((a), sizeof(T) * (n), _Alignof(T)))


#ifdef __cplusplus
}
#endif

#endif /* NTH_MEM_ALLOCATOR_H */