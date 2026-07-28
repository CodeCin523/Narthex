#ifndef NTH_MEM_ARENA_H
#define NTH_MEM_ARENA_H

#ifdef __cplusplus
extern "C" {
#endif

#include <narthex/mem/span.h>


typedef struct NthArena {
    NthSpan span;
    usize offset;
} NthArena;


b8 nth_setup_arena(NthArena *arena, NthSpan span);
void nth_teardown_arena(NthArena *arena);

void *nth_arena_alloc(NthArena *arena, usize size, usize align);
uptr nth_arena_mark(NthArena *arena);
void nth_arena_restore(NthArena *arena, uptr mark);
void nth_arena_clean(NthArena *arena);


#ifdef __cplusplus
}
#endif

#endif /* NTH_MEM_ARENA_H */