#ifndef NTH_MEM_DYN_ARENA_H
#define NTH_MEM_DYN_ARENA_H

#include <narthex/mem/span.h>
#include <narthex/nth_result.h>
#include <narthex/utils/api.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef struct NthDynArena {
    NthSpan *spans;
    nth_usize span_count;
    nth_usize span_capacity;
    nth_usize span_idx;
    nth_usize offset;
} NthDynArena;

typedef struct NthDynArenaMark {
    nth_u64 v;
} NthDynArenaMark;


NTH_API NthResult nth_setup_dyn_arena(NthDynArena *arena, NthSpan span);
NTH_API void nth_teardown_dyn_arena(NthDynArena *arena);

NTH_API nth_b8 nth_dyn_arena_grow(NthDynArena *arena, NthSpan span);
NTH_API NthSpan nth_dyn_arena_shrink(NthDynArena *arena);

NTH_API void *nth_dyn_arena_alloc(NthDynArena *arena, nth_usize size, nth_usize align);
NTH_API NthDynArenaMark nth_dyn_arena_mark(NthDynArena *arena);
NTH_API nth_b8 nth_dyn_arena_restore(NthDynArena *arena, NthDynArenaMark mark);
NTH_API void nth_dyn_arena_clean(NthDynArena *arena);


#ifdef __cplusplus
}
#endif

#endif /* NTH_MEM_DYN_ARENA_H */