#ifndef NTH_MEM_DYN_ARENA_H
#define NTH_MEM_DYN_ARENA_H

#include <narthex/mem/span.h>

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


nth_b8 nth_setup_dyn_arena(NthDynArena *arena, NthSpan span);
void nth_teardown_dyn_arena(NthDynArena *arena);

nth_b8 nth_dyn_arena_grow(NthDynArena *arena, NthSpan span);
NthSpan nth_dyn_arena_shrink(NthDynArena *arena);

void *nth_dyn_arena_alloc(NthDynArena *arena, nth_usize size, nth_usize align);
nth_uptr nth_dyn_arena_mark(NthDynArena *arena);
void nth_dyn_arena_restore(NthDynArena *arena, nth_uptr mark);
void nth_dyn_arena_clean(NthDynArena *arena);


#ifdef __cplusplus
}
#endif

#endif /* NTH_MEM_DYN_ARENA_H */