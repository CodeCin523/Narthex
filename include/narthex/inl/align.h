#ifndef NTH_INL_ALIGN_H
#define NTH_INL_ALIGN_H

#include <narthex/utils/check.h>
#include <narthex/nth_types.h>

#ifdef __cplusplus
extern "C" {
#endif


static inline nth_b8 nth_is_pow2(nth_uptr v) {
    return v != 0 && (v & (v - 1)) == 0;
}
static inline nth_b8 nth_is_aligned(nth_uptr v, nth_usize align) {
    NTH_DASSERT(NTH_LIKELY(nth_is_pow2(align)));
    return (v & (align - 1)) == 0;
}

static inline nth_uptr nth_align_up(nth_uptr v, nth_usize align) {
    NTH_DASSERT(NTH_LIKELY(nth_is_pow2(align)));

    const nth_uptr mask = align - 1;
    NTH_DASSERT(NTH_LIKELY(v <= NTH_UPTR_MAX - mask));

    return (v + mask) & ~mask;
}
static inline nth_uptr nth_align_down(nth_uptr v, nth_usize align) {
    NTH_DASSERT(NTH_LIKELY(nth_is_pow2(align)));

    return v & ~(align - 1);
}


#ifdef __cplusplus
}
#endif

#endif /* NTH_INL_ALIGN_H */