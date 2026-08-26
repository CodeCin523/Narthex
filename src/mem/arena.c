#include <narthex/mem/arena.h>

#include <narthex/utils/check.h>
#include <narthex/inl/align.h>

#include "poison.h"


NthResult nth_setup_arena(NthArena *arena, NthSpan span) {
    NTH_DASSERT(NTH_LIKELY(arena != NULL));
    NTH_DASSERT(NTH_LIKELY(span.base != NULL && span.size != 0));

    arena->span = span;
    arena->offset = 0;

    nth_poison_dead(span.base, span.size);
    return NTH_RESULT_OK;
}
void nth_teardown_arena(NthArena *arena) {
    NTH_DASSERT(NTH_LIKELY(arena != NULL));

    nth_poison_disown(arena->span.base, arena->span.size);

    arena->span = (NthSpan){0};
    arena->offset = 0;
}

void *nth_arena_alloc(NthArena *arena, nth_usize size, nth_usize align) {
    NTH_DASSERT(NTH_LIKELY(arena != NULL));
    NTH_DASSERT(NTH_LIKELY(arena->span.base != NULL));
    NTH_DASSERT(NTH_LIKELY(size > 0));
    NTH_DASSERT(NTH_LIKELY(nth_is_pow2(align)));

    nth_usize pad  = nth_align_pad((nth_uptr)arena->span.base + arena->offset, align);
    nth_usize left = arena->span.size - arena->offset;

    if (NTH_UNLIKELY(pad > left)) return NULL;
    if (NTH_UNLIKELY(size > left - pad)) return NULL;

    nth_usize at = arena->offset + pad;
    arena->offset = at + size;

    nth_poison_live(arena->span.base + at, size);

    return arena->span.base + at;
}
NthArenaMark nth_arena_mark(NthArena *arena) {
    NTH_DASSERT(NTH_LIKELY(arena != NULL));
    NTH_DASSERT(NTH_LIKELY(arena->span.base != NULL));

    return (NthArenaMark){arena->offset}; // would like to mask or xor the value
}
nth_b8 nth_arena_restore(NthArena *arena, NthArenaMark mark) {
    NTH_DASSERT(NTH_LIKELY(arena != NULL));
    NTH_DASSERT(NTH_LIKELY(arena->span.base != NULL));
    
    if(NTH_UNLIKELY(mark.v > arena->offset))
        return NTH_FALSE;

    nth_poison_dead(arena->span.base + mark.v, arena->offset - mark.v);

    arena->offset = mark.v;
    return NTH_TRUE;
}
void nth_arena_clean(NthArena *arena) {
    NTH_DASSERT(NTH_LIKELY(arena != NULL));
    NTH_DASSERT(NTH_LIKELY(arena->span.base != NULL));

    nth_poison_dead(arena->span.base, arena->offset);

    arena->offset = 0;
}