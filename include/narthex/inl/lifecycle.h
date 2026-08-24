#ifndef NTH_INL_LIFECYCLE_H
#define NTH_INL_LIFECYCLE_H

#include <narthex/nth_types.h>
#include <narthex/nth_result.h>
#include <narthex/inl/atomic.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef nth_u32 NthLifecycle;
enum {
    NTH_LIFECYCLE_DEAD = 0,
    NTH_LIFECYCLE_STARTING = 1,
    NTH_LIFECYCLE_ALIVE = 2,
    NTH_LIFECYCLE_STOPPING = 3
};


static inline nth_b8 nth_lifecycle_is_alive(const volatile NthLifecycle *lc) {
    return (nth_b8)(nth_atomic_load_acquire_u32(lc) == NTH_LIFECYCLE_ALIVE);
}

static inline NthResult nth_lifecycle_begin_init(volatile NthLifecycle *lc) {
    NthLifecycle expected = NTH_LIFECYCLE_DEAD;
    if(nth_atomic_cas_acq_rel_u32(lc, &expected, NTH_LIFECYCLE_STARTING))
        return NTH_RESULT_OK;

    return expected == NTH_LIFECYCLE_ALIVE? NTH_RESULT_ALREADY_INITIALIZED : NTH_RESULT_NOT_READY;
}
static inline void nth_lifecycle_end_init(volatile NthLifecycle *lc) {
    nth_atomic_store_release_u32(lc, NTH_LIFECYCLE_ALIVE);
}
static inline void nth_lifecycle_fail_init(volatile NthLifecycle *lc) {
    nth_atomic_store_release_u32(lc, NTH_LIFECYCLE_DEAD);

}

static inline NthResult nth_lifecycle_begin_term(volatile NthLifecycle *lc) {
    NthLifecycle expected = NTH_LIFECYCLE_ALIVE;
    if(nth_atomic_cas_acq_rel_u32(lc, &expected, NTH_LIFECYCLE_STOPPING))
        return NTH_RESULT_OK;

    return expected == NTH_LIFECYCLE_DEAD? NTH_RESULT_NOT_INITIALIZED : NTH_RESULT_NOT_READY;
}
static inline void nth_lifecycle_end_term(volatile NthLifecycle *lc) {
    nth_atomic_store_release_u32(lc, NTH_LIFECYCLE_DEAD);
}


#ifdef __cplusplus
}
#endif

#endif /* NTH_INL_LIFECYCLE_H */