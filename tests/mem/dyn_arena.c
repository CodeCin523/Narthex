#define NTH_SHORT_TYPES

#include "nth_test.h"

#include <narthex/mem/dyn_arena.h>


static b8 test_alloc_basic(void) {
    u8 buf[256];
    NthDynArena arena;
    nth_setup_dyn_arena(&arena, (NthSpan){buf, 256});

    void *p = nth_dyn_arena_alloc(&arena, 16, 1);
    NTH_TEST_ASSERT(p != NULL);
    NTH_TEST_ASSERT(p == buf);

    nth_teardown_dyn_arena(&arena);
    return NTH_TRUE;
}

static b8 test_alloc_spill(void) {
    u8 buf0[64];
    u8 buf1[64];
    NthDynArena arena;
    nth_setup_dyn_arena(&arena, (NthSpan){buf0, 64});
    nth_dyn_arena_grow(&arena, (NthSpan){buf1, 64});

    void *p0 = nth_dyn_arena_alloc(&arena, 64, 1);
    NTH_TEST_ASSERT(p0 == buf0);

    void *p1 = nth_dyn_arena_alloc(&arena, 8, 1);
    NTH_TEST_ASSERT(p1 != NULL);
    NTH_TEST_ASSERT((u8 *)p1 >= buf1 && (u8 *)p1 < buf1 + 64);

    nth_teardown_dyn_arena(&arena);
    return NTH_TRUE;
}

static b8 test_alloc_oom(void) {
    u8 buf[64];
    NthDynArena arena;
    nth_setup_dyn_arena(&arena, (NthSpan){buf, 64});

    NTH_TEST_ASSERT(nth_dyn_arena_alloc(&arena, 64, 1) != NULL);
    NTH_TEST_ASSERT(nth_dyn_arena_alloc(&arena,  1, 1) == NULL);

    nth_teardown_dyn_arena(&arena);
    return NTH_TRUE;
}

static b8 test_grow_shrink(void) {
    u8 buf0[64];
    u8 buf1[64];
    NthDynArena arena;
    nth_setup_dyn_arena(&arena, (NthSpan){buf0, 64});

    NTH_TEST_ASSERT(nth_dyn_arena_grow(&arena, (NthSpan){buf1, 64}));
    NTH_TEST_ASSERT(arena.span_count == 2);

    NthSpan removed = nth_dyn_arena_shrink(&arena);
    NTH_TEST_ASSERT(removed.pool     == buf1);
    NTH_TEST_ASSERT(removed.capacity == 64);
    NTH_TEST_ASSERT(arena.span_count == 1);

    nth_teardown_dyn_arena(&arena);
    return NTH_TRUE;
}

static b8 test_mark_restore(void) {
    u8 buf0[64];
    u8 buf1[64];
    NthDynArena arena;
    nth_setup_dyn_arena(&arena, (NthSpan){buf0, 64});
    nth_dyn_arena_grow(&arena, (NthSpan){buf1, 64});

    nth_dyn_arena_alloc(&arena, 64, 1);
    nth_dyn_arena_alloc(&arena, 16, 1);

    uptr mark = nth_dyn_arena_mark(&arena);

    nth_dyn_arena_alloc(&arena, 16, 1);
    nth_dyn_arena_restore(&arena, mark);

    NTH_TEST_ASSERT(arena.span_idx == 1);
    NTH_TEST_ASSERT(arena.offset   == 16);

    nth_teardown_dyn_arena(&arena);
    return NTH_TRUE;
}

static b8 test_clean(void) {
    u8 buf0[64];
    u8 buf1[64];
    NthDynArena arena;
    nth_setup_dyn_arena(&arena, (NthSpan){buf0, 64});
    nth_dyn_arena_grow(&arena, (NthSpan){buf1, 64});

    nth_dyn_arena_alloc(&arena, 64, 1);
    nth_dyn_arena_alloc(&arena, 32, 1);
    nth_dyn_arena_clean(&arena);

    NTH_TEST_ASSERT(arena.span_idx == 0);
    NTH_TEST_ASSERT(arena.offset   == 0);

    nth_teardown_dyn_arena(&arena);
    return NTH_TRUE;
}


int main(void) {
    NthTest tests[] = {
        { "alloc/basic",  test_alloc_basic  },
        { "alloc/spill",  test_alloc_spill  },
        { "alloc/oom",    test_alloc_oom    },
        { "grow_shrink",  test_grow_shrink  },
        { "mark_restore", test_mark_restore },
        { "clean",        test_clean        },
    };

    return NTH_RUN_TESTS(tests);
}
