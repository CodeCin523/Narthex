#ifndef NTH_MEM_ARENA_H
#define NTH_MEM_ARENA_H

#include <narthex/mem/span.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef struct NthArena {
    NthSpan span;
    nth_usize offset;
} NthArena;


void nth_setup_arena(NthArena *arena, NthSpan span);
void nth_teardown_arena(NthArena *arena);

void *nth_arena_alloc(NthArena *arena, nth_usize size, nth_usize align);
nth_uptr nth_arena_mark(NthArena *arena);
void nth_arena_restore(NthArena *arena, nth_uptr mark);
void nth_arena_clean(NthArena *arena);


#ifdef __cplusplus
}
#endif

#endif /* NTH_MEM_ARENA_H */