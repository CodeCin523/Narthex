#ifndef NTH_MEM_ARENA_H
#define NTH_MEM_ARENA_H

#include <narthex/mem/span.h>
#include <narthex/nth_result.h>
#include <narthex/utils/api.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef struct NthArena {
    NthSpan span;
    nth_usize offset;
} NthArena;

typedef struct NthArenaMark {
    nth_u64 v;
} NthArenaMark;


NTH_API NthResult nth_setup_arena(NthArena *arena, NthSpan span);
NTH_API void nth_teardown_arena(NthArena *arena);

NTH_API void *nth_arena_alloc(NthArena *arena, nth_usize size, nth_usize align);
NTH_API NthArenaMark nth_arena_mark(NthArena *arena);
NTH_API nth_b8 nth_arena_restore(NthArena *arena, NthArenaMark mark);
NTH_API void nth_arena_clean(NthArena *arena);


#ifdef __cplusplus
}
#endif

#endif /* NTH_MEM_ARENA_H */