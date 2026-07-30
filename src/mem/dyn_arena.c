#include <narthex/mem/dyn_arena.h>

#include <narthex/utils/check.h>
#include <narthex/inl/align.h>

#include "poison.h"

#include <stdlib.h>


// 65535 spans and 256TB offset
#define NTH_DYN_MARK_OFF_BITS 48
#define NTH_DYN_MARK_OFF_MASK ((nth_u64)0xFFFFFFFFFFFF)


#if NTH_POISON || NTH_HAS_ASAN
static void nth_dyn_poison_back_to(NthDynArena *arena, nth_usize idx, nth_usize off) {
    if (arena->span_count == 0)
        return;

    if (idx == arena->span_idx) {
        nth_poison_dead(arena->spans[idx].base + off, arena->offset - off);
        return;
    }

    nth_poison_dead(arena->spans[idx].base + off, arena->spans[idx].size - off);

    for (nth_usize i = idx + 1; i < arena->span_idx; i++)
        nth_poison_dead(arena->spans[i].base, arena->spans[i].size);

    nth_poison_dead(arena->spans[arena->span_idx].base, arena->offset);
}
#else
#define nth_dyn_poison_back_to(arena, idx, off) ((void)0)
#endif


nth_b8 nth_setup_dyn_arena(NthDynArena *arena, NthSpan span) {
    NTH_DASSERT(NTH_LIKELY(arena != NULL));
    NTH_DASSERT(NTH_LIKELY(span.base != NULL && span.size != 0));

    NthSpan *tmp = malloc(4 * sizeof(NthSpan));
    if(NTH_UNLIKELY(tmp == NULL))
        return NTH_FALSE;

    arena->spans = tmp;
    arena->span_count = 1;
    arena->span_capacity = 4;

    arena->spans[0] = span;

    arena->span_idx = 0;
    arena->offset = 0;

    nth_poison_dead(span.base, span.size);

    return NTH_TRUE;
}
void nth_teardown_dyn_arena(NthDynArena *arena) {
    NTH_DASSERT(NTH_LIKELY(arena != NULL));

    if(arena->spans != NULL) {
        for(nth_usize i = 0; i < arena->span_count; i++)
            nth_poison_disown(arena->spans[i].base, arena->spans[i].size);

        free(arena->spans);
    }

    arena->spans = NULL;
    arena->span_count = 0;
    arena->span_capacity = 0;
    arena->span_idx = 0;
    arena->offset = 0;
}

nth_b8 nth_dyn_arena_grow(NthDynArena *arena, NthSpan span) {
    NTH_DASSERT(NTH_LIKELY(arena != NULL));
    NTH_DASSERT(NTH_LIKELY(arena->spans != NULL));
    NTH_DASSERT(NTH_LIKELY(span.base != NULL && span.size != 0));

    if(NTH_UNLIKELY(arena->span_count + 1 >= arena->span_capacity)) {
        nth_usize ncapacity = arena->span_capacity * 2;

        NthSpan *tmp = realloc(arena->spans, ncapacity * sizeof(NthSpan));
        if(NTH_UNLIKELY(tmp == NULL))
            return NTH_FALSE;

        arena->spans = tmp;
        arena->span_capacity = ncapacity;
    }

    arena->spans[arena->span_count] = span;
    arena->span_count++;

    nth_poison_dead(span.base, span.size);

    return NTH_TRUE;
}
NthSpan nth_dyn_arena_shrink(NthDynArena *arena) {
    NTH_DASSERT(NTH_LIKELY(arena != NULL));
    NTH_DASSERT(NTH_LIKELY(arena->spans != NULL));

    if (arena->span_count == 0)
        return (NthSpan){0};

    nth_usize last = arena->span_count - 1;

    if (arena->span_idx == last && !(arena->span_idx == 0 && arena->offset == 0))
        return (NthSpan){0};

    NthSpan ret = arena->spans[last];
    arena->spans[last] = (NthSpan){0};
    arena->span_count = last;

    nth_poison_disown(ret.base, ret.size);

    return ret;
}

void *nth_dyn_arena_alloc(NthDynArena *arena, nth_usize size, nth_usize align) {
    NTH_DASSERT(NTH_LIKELY(arena != NULL));
    NTH_DASSERT(NTH_LIKELY(arena->spans != NULL));
    NTH_DASSERT(NTH_LIKELY(size > 0));
    NTH_DASSERT(NTH_LIKELY(nth_is_pow2(align)));

    if (NTH_UNLIKELY(arena->span_count == 0))
        return NULL;

    NthSpan cur = arena->spans[arena->span_idx];

    nth_usize pad  = nth_align_pad((nth_uptr)cur.base + arena->offset, align);
    nth_usize left = cur.size - arena->offset;

    if (NTH_LIKELY(pad <= left && size <= left - pad)) {
        nth_usize at  = arena->offset + pad;
        arena->offset = at + size;
        nth_poison_live(cur.base + at, size);
        return cur.base + at;
    }

    for (nth_usize i = arena->span_idx + 1; i < arena->span_count; i++) {
        NthSpan s = arena->spans[i];
        nth_usize p = nth_align_pad((nth_uptr)s.base, align);

        if (p > s.size) continue;
        if (size > s.size - p) continue;

        nth_usize next = arena->span_idx + 1;
        if (i != next) {
            NthSpan tmp = arena->spans[next];
            arena->spans[next] = arena->spans[i];
            arena->spans[i] = tmp;
        }

        arena->span_idx = next;
        arena->offset = p + size;
        nth_poison_live(arena->spans[next].base + p, size);
        return arena->spans[next].base + p;
    }

    return NULL;
}
NthDynArenaMark nth_dyn_arena_mark(NthDynArena *arena) {
    NTH_DASSERT(NTH_LIKELY(arena != NULL));
    NTH_DASSERT(NTH_LIKELY(arena->spans != NULL));

    return (NthDynArenaMark){
        (nth_u64)arena->span_idx << NTH_DYN_MARK_OFF_BITS | (nth_u64)arena->offset
    };
}
nth_b8 nth_dyn_arena_restore(NthDynArena *arena, NthDynArenaMark mark) {
    NTH_DASSERT(NTH_LIKELY(arena != NULL));
    NTH_DASSERT(NTH_LIKELY(arena->spans != NULL));

    nth_u64 idx = mark.v >> NTH_DYN_MARK_OFF_BITS;
    nth_u64 off = mark.v & NTH_DYN_MARK_OFF_MASK;

    if(NTH_UNLIKELY(idx > arena->span_idx))
        return NTH_FALSE;

    nth_u64 limit = (idx == arena->span_idx) ? arena->offset : arena->spans[idx].size;

    if(NTH_UNLIKELY(off > limit))
        return NTH_FALSE;

    nth_dyn_poison_back_to(arena, (nth_usize)idx, (nth_usize)off);

    arena->span_idx = idx;
    arena->offset = off;
    return NTH_TRUE;
}
void nth_dyn_arena_clean(NthDynArena *arena) {
    NTH_DASSERT(NTH_LIKELY(arena != NULL));
    NTH_DASSERT(NTH_LIKELY(arena->spans != NULL));

    nth_dyn_poison_back_to(arena, 0, 0);

    arena->offset = 0;
    arena->span_idx = 0;
}