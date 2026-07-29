#define NTH_SHORT_TYPES

#include <narthex/mem/dyn_arena.h>

#include <narthex/utils/check.h>
#include <narthex/inl/align.h>

#include <stdlib.h>


b8 nth_setup_dyn_arena(NthDynArena *arena, NthSpan span) {
    NTH_DASSERT(NTH_LIKELY(arena != NULL));
    NTH_DASSERT(NTH_LIKELY(span.pool != NULL && span.capacity != 0));

    NthSpan *tmp = malloc(4 * sizeof(NthSpan));
    if(NTH_UNLIKELY(tmp == NULL))
        return NTH_FALSE;

    arena->spans = tmp;
    arena->span_count = 1;
    arena->span_capacity = 4;

    arena->spans[0] = span;

    arena->span_idx = 0;
    arena->offset = 0;

    return NTH_TRUE;
}
void nth_teardown_dyn_arena(NthDynArena *arena) {
    NTH_DASSERT(NTH_LIKELY(arena != NULL));

    if(arena->spans != NULL)
        free(arena->spans);

    arena->spans = NULL;
    arena->span_count = 0;
    arena->span_capacity = 0;
    arena->span_idx = 0;
    arena->offset = 0;
}

b8 nth_dyn_arena_grow(NthDynArena *arena, NthSpan span) {
    NTH_DASSERT(NTH_LIKELY(arena != NULL));
    NTH_DASSERT(NTH_LIKELY(arena->spans != NULL));
    NTH_DASSERT(NTH_LIKELY(span.pool != NULL && span.capacity != 0));

    if(NTH_UNLIKELY(arena->span_count + 1 >= arena->span_capacity)) {
        usize ncapacity = arena->span_capacity * 2;

        NthSpan *tmp = realloc(arena->spans, ncapacity * sizeof(NthSpan));
        if(NTH_UNLIKELY(tmp == NULL))
            return NTH_FALSE;

        arena->spans = tmp;
        arena->span_capacity = ncapacity;
    }

    arena->spans[arena->span_count] = span;
    arena->span_count++;

    return NTH_TRUE;
}
NthSpan nth_dyn_arena_shrink(NthDynArena *arena) {
    NTH_DASSERT(NTH_LIKELY(arena != NULL));
    NTH_DASSERT(NTH_LIKELY(arena->spans != NULL));

    NthSpan ret = {NULL, 0};

    if(NTH_LIKELY(arena->span_count > 0)) {
        ret = arena->spans[--arena->span_count];
        arena->spans[arena->span_count] = (NthSpan){0};
    }

    return ret;
}

void *nth_dyn_arena_alloc(NthDynArena *arena, usize size, usize align) {
    NTH_DASSERT(NTH_LIKELY(arena != NULL));
    NTH_DASSERT(NTH_LIKELY(arena->spans != NULL));
    NTH_DASSERT(NTH_LIKELY(size > 0));
    NTH_DASSERT(NTH_LIKELY(nth_is_pow2(align)));

    NthSpan cspan = arena->spans[arena->span_idx];

    uptr aligned = nth_align_up((uptr)cspan.pool + arena->offset, align);

    if(NTH_LIKELY(aligned + size <= (uptr)cspan.pool + cspan.capacity)) {
        uptr offset = aligned - (uptr)cspan.pool;
        arena->offset = offset + size;

        return (void *)aligned;
    } else {
        if(NTH_UNLIKELY(arena->span_idx + 1 >= arena->span_count))
            return NULL;

        cspan = arena->spans[arena->span_idx + 1];

        uptr aligned = nth_align_up((uptr)cspan.pool, align);

        if(NTH_UNLIKELY(aligned + size > (uptr)cspan.pool + cspan.capacity))
            return NULL;

        ++arena->span_idx;
        arena->offset = aligned + size - (uptr)cspan.pool;

        return (void *)aligned;
    }
}
uptr nth_dyn_arena_mark(NthDynArena *arena) {
    NTH_DASSERT(NTH_LIKELY(arena != NULL));
    NTH_DASSERT(NTH_LIKELY(arena->spans != NULL));

    uptr mark = 0;
    
    for(usize i = 0; i < arena->span_idx; i++) {
        mark += arena->spans[i].capacity;
    }

    mark += arena->offset;

    return mark;
}
void nth_dyn_arena_restore(NthDynArena *arena, uptr mark) {
    NTH_DASSERT(NTH_LIKELY(arena != NULL));
    NTH_DASSERT(NTH_LIKELY(arena->spans != NULL));

    for(usize i = 0; i < arena->span_count; i++) {

        if(mark > arena->spans[i].capacity)
            mark -= arena->spans[i].capacity;
        else {
            arena->span_idx = i;
            arena->offset = mark;

            return;
        }
    }
}
void nth_dyn_arena_clean(NthDynArena *arena) {
    NTH_DASSERT(NTH_LIKELY(arena != NULL));
    NTH_DASSERT(NTH_LIKELY(arena->spans != NULL));

    arena->offset = 0;
    arena->span_idx = 0;
}