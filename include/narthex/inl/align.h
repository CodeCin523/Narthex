#ifndef NTH_INL_ALIGN_H
#define NTH_INL_ALIGN_H

#ifdef __cplusplus
extern "C" {
#endif

#include <narthex/utils/check.h>
#include <narthex/nth_types.h>


static inline b8 nth_is_pow2(uptr v) {
    return v != 0 && (v & (v - 1)) == 0;
}
static inline b8 nth_is_aligned(uptr v, usize align) {
    NTH_DASSERT(NTH_LIKELY(nth_is_pow2(align)));
    return (v & (align - 1)) == 0;
}

static inline uptr nth_align_up(uptr v, usize align) {
    NTH_DASSERT(NTH_LIKELY(nth_is_pow2(align)));

    const uptr mask = align - 1;
    NTH_DASSERT(NTH_LIKELY(v <= UPTR_MAX - mask));

    return (v + mask) & ~mask;
}
static inline uptr nth_align_down(uptr v, usize align) {
    NTH_DASSERT(NTH_LIKELY(nth_is_pow2(align)));

    return v & ~(align - 1);
}


#ifdef __cplusplus
}
#endif

#endif /* NTH_INL_ALIGN_H */