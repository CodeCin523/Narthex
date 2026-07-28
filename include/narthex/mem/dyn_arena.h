#ifndef NTH_MEM_DYN_ARENA_H
#define NTH_MEM_DYN_ARENA_H

#ifdef __cplusplus
extern "C" {
#endif

#include <narthex/mem/span.h>


typedef struct NthDynArena {
    NthSpan *spans;
    usize span_count;
    usize span_capacity;
    usize span_idx;
    usize offset;
} NthDynArena;


b8 nth_setup_dyn_arena(NthDynArena *arena, NthSpan span);
void nth_teardown_dyn_arena(NthDynArena *arena);

b8 nth_dyn_arena_grow(NthDynArena *arena, NthSpan span);
NthSpan nth_dyn_arena_shrink(NthDynArena *arena);

void *nth_dyn_arena_alloc(NthDynArena *arena, usize size, usize align);
uptr nth_dyn_arena_mark(NthDynArena *arena);
void nth_dyn_arena_restore(NthDynArena *arena, uptr mark);
void nth_dyn_arena_clean(NthDynArena *arena);


#ifdef __cplusplus
}
#endif

#endif /* NTH_MEM_DYN_ARENA_H */