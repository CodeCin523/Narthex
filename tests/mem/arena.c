#include "nth_test.h"

#include <narthex/mem/arena.h>


static b8 test_alloc_basic(void) {
    u8 buf[256];
    NthArena arena;
    nth_setup_arena(&arena, (NthSpan){buf, 256});

    void *p = nth_arena_alloc(&arena, 16, 1);
    NTH_TEST_ASSERT(p != NULL);
    NTH_TEST_ASSERT(p == buf);

    nth_teardown_arena(&arena);
    return NTH_TRUE;
}

static b8 test_alloc_align(void) {
    u8 buf[256];
    NthArena arena;
    nth_setup_arena(&arena, (NthSpan){buf, 256});

    nth_arena_alloc(&arena, 1, 1);

    void *p = nth_arena_alloc(&arena, 8, 8);
    NTH_TEST_ASSERT(p != NULL);
    NTH_TEST_ASSERT(((uptr)p % 8) == 0);

    nth_teardown_arena(&arena);
    return NTH_TRUE;
}

static b8 test_alloc_no_overlap(void) {
    u8 buf[256];
    NthArena arena;
    nth_setup_arena(&arena, (NthSpan){buf, 256});

    u8 *a = nth_arena_alloc(&arena, 32, 1);
    u8 *b = nth_arena_alloc(&arena, 32, 1);
    NTH_TEST_ASSERT(a != NULL && b != NULL);
    NTH_TEST_ASSERT(b >= a + 32);

    nth_teardown_arena(&arena);
    return NTH_TRUE;
}

static b8 test_alloc_oom(void) {
    u8 buf[64];
    NthArena arena;
    nth_setup_arena(&arena, (NthSpan){buf, 64});

    NTH_TEST_ASSERT(nth_arena_alloc(&arena, 64, 1) != NULL);
    NTH_TEST_ASSERT(nth_arena_alloc(&arena,  1, 1) == NULL);

    nth_teardown_arena(&arena);
    return NTH_TRUE;
}

static b8 test_mark_restore(void) {
    u8 buf[256];
    NthArena arena;
    nth_setup_arena(&arena, (NthSpan){buf, 256});

    nth_arena_alloc(&arena, 32, 1);
    uptr mark = nth_arena_mark(&arena);

    nth_arena_alloc(&arena, 64, 1);
    nth_arena_restore(&arena, mark);

    void *p = nth_arena_alloc(&arena, 8, 1);
    NTH_TEST_ASSERT((uptr)p - (uptr)buf == 32);

    nth_teardown_arena(&arena);
    return NTH_TRUE;
}

static b8 test_clean(void) {
    u8 buf[256];
    NthArena arena;
    nth_setup_arena(&arena, (NthSpan){buf, 256});

    nth_arena_alloc(&arena, 128, 1);
    nth_arena_clean(&arena);

    void *p = nth_arena_alloc(&arena, 8, 1);
    NTH_TEST_ASSERT(p == buf);

    nth_teardown_arena(&arena);
    return NTH_TRUE;
}


int main(void) {
    NthTest tests[] = {
        { "alloc/basic",      test_alloc_basic      },
        { "alloc/align",      test_alloc_align      },
        { "alloc/no_overlap", test_alloc_no_overlap },
        { "alloc/oom",        test_alloc_oom        },
        { "mark_restore",     test_mark_restore     },
        { "clean",            test_clean            },
    };

    return NTH_RUN_TESTS(tests);
}
