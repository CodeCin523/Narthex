#include <narthex/mem/arena.h>

#include <narthex/utils/check.h>
#include <narthex/inl/align.h>


b8 nth_setup_arena(NthArena *arena, NthSpan span) {
    NTH_DASSERT(NTH_LIKELY(arena != NULL));
    NTH_DASSERT(NTH_LIKELY(span.pool != NULL && span.capacity != 0));

    arena->span = span;
    arena->offset = 0;

    return NTH_TRUE;
}
void nth_teardown_arena(NthArena *arena) {
    NTH_DASSERT(NTH_LIKELY(arena != NULL));

    arena->span = (NthSpan){0};
    arena->offset = 0;
}

void *nth_arena_alloc(NthArena *arena, usize size, usize align) {
    NTH_DASSERT(NTH_LIKELY(arena != NULL));
    NTH_DASSERT(NTH_LIKELY(arena->span.pool != NULL));
    NTH_DASSERT(NTH_LIKELY(size > 0));
    NTH_DASSERT(NTH_LIKELY(nth_is_pow2(align)));

    uptr aligned = nth_align_up((uptr)arena->span.pool + arena->offset, align);

    if(NTH_UNLIKELY(aligned + size > (uptr)arena->span.pool + arena->span.capacity))
        return NULL;

    arena->offset = (aligned - (uptr)arena->span.pool) + size;

    return (void *)aligned;
}
uptr nth_arena_mark(NthArena *arena) {
    NTH_DASSERT(NTH_LIKELY(arena != NULL));
    NTH_DASSERT(NTH_LIKELY(arena->span.pool != NULL));

    return arena->offset; // would like to mask or xor the value
}
void nth_arena_restore(NthArena *arena, uptr mark) {
    NTH_DASSERT(NTH_LIKELY(arena != NULL));
    NTH_DASSERT(NTH_LIKELY(arena->span.pool != NULL));
    
    arena->offset = mark;
}
void nth_arena_clean(NthArena *arena) {
    NTH_DASSERT(NTH_LIKELY(arena != NULL));
    NTH_DASSERT(NTH_LIKELY(arena->span.pool != NULL));

    arena->offset = 0;
}