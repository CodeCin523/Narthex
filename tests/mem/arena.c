#include "nth_test.h"

#include <narthex/mem/arena.h>
#include <narthex/inl/align.h>

#include <string.h>

#define POOL      16384
#define MAX_BLK   256
#define STRESS_IT 120000

static nth_u8 g_pool[POOL];

static NthSpan span_of(nth_usize size, nth_usize align) {
    nth_uptr p = nth_align_up((nth_uptr)g_pool, align);
    return (NthSpan){ (nth_u8 *)p, size };
}


static nth_b8 test_alloc_basic(void) {
    NthArena a;
    NthSpan  s = span_of(256, 64);
    nth_setup_arena(&a, s);

    void *p = nth_arena_alloc(&a, 16, 1);
    NTH_TEST_ASSERT(p == s.base);
    NTH_TEST_ASSERT(a.offset == 16);

    nth_teardown_arena(&a);
    return NTH_TRUE;
}

static nth_b8 test_alignment_all(void) {
    for (nth_usize sh = 0; sh <= 12; sh++) {
        nth_usize align = (nth_usize)1 << sh;

        for (nth_usize skew = 0; skew < 8; skew++) {
            NthArena a;
            nth_setup_arena(&a, span_of(8192, 4096));

            if (skew != 0)
                NTH_TEST_ASSERT(nth_arena_alloc(&a, skew, 1) != NULL);

            void *p = nth_arena_alloc(&a, 8, align);
            NTH_TEST_ASSERT(p != NULL);
            NTH_TEST_ASSERT(((nth_uptr)p & (nth_uptr)(align - 1)) == 0);

            nth_teardown_arena(&a);
        }
    }
    return NTH_TRUE;
}

static nth_b8 test_no_overlap_patterned(void) {
    NthArena a;
    NthSpan  s = span_of(1024, 64);
    nth_setup_arena(&a, s);

    nth_u8   *blk[MAX_BLK];
    nth_usize len[MAX_BLK];
    nth_usize n = 0;

    while (n < MAX_BLK) {
        nth_usize sz = 1 + (n % 13);
        nth_usize al = (nth_usize)1 << (n % 6);

        nth_u8 *p = nth_arena_alloc(&a, sz, al);
        if (p == NULL)
            break;

        NTH_TEST_ASSERT((nth_uptr)p >= (nth_uptr)s.base);
        NTH_TEST_ASSERT((nth_uptr)p + sz <= (nth_uptr)s.base + s.size);
        NTH_TEST_ASSERT(((nth_uptr)p & (nth_uptr)(al - 1)) == 0);

        memset(p, (int)(n & 0xFF), sz);
        blk[n] = p;
        len[n] = sz;
        n++;
    }

    NTH_TEST_ASSERT(n > 16);

    for (nth_usize i = 0; i < n; i++)
        for (nth_usize k = 0; k < len[i]; k++)
            NTH_TEST_ASSERT(blk[i][k] == (nth_u8)(i & 0xFF));

    nth_teardown_arena(&a);
    return NTH_TRUE;
}

static nth_b8 test_exact_fill(void) {
    NthArena a;
    NthSpan  s = span_of(128, 64);
    nth_setup_arena(&a, s);

    NTH_TEST_ASSERT(nth_arena_alloc(&a, 128, 1) == s.base);
    NTH_TEST_ASSERT(a.offset == 128);
    NTH_TEST_ASSERT(nth_arena_alloc(&a, 1, 1) == NULL);
    NTH_TEST_ASSERT(a.offset == 128);

    nth_teardown_arena(&a);
    return NTH_TRUE;
}

static nth_b8 test_boundary_sizes(void) {
    const nth_usize cap = 128;

    for (nth_usize d = 0; d < 3; d++) {
        NthArena a;
        nth_setup_arena(&a, span_of(cap, 64));

        nth_usize req = cap - 1 + d;
        void     *p   = nth_arena_alloc(&a, req, 1);

        if (req <= cap) {
            NTH_TEST_ASSERT(p != NULL);
            NTH_TEST_ASSERT(a.offset == req);
        } else {
            NTH_TEST_ASSERT(p == NULL);
            NTH_TEST_ASSERT(a.offset == 0);
        }

        nth_teardown_arena(&a);
    }
    return NTH_TRUE;
}

static nth_b8 test_overflow_size_max(void) {
    const nth_usize aligns[] = { 1, 8, 64, 4096 };

    for (nth_usize i = 0; i < 4; i++) {
        for (nth_usize k = 0; k < 16; k++) {
            NthArena a;
            nth_setup_arena(&a, span_of(64, 4096));

            NTH_TEST_ASSERT(nth_arena_alloc(&a, NTH_USIZE_MAX - k, aligns[i]) == NULL);
            NTH_TEST_ASSERT(a.offset == 0);

            nth_teardown_arena(&a);
        }
    }
    return NTH_TRUE;
}

static nth_b8 test_overflow_after_partial(void) {
    NthArena a;
    nth_setup_arena(&a, span_of(64, 4096));

    NTH_TEST_ASSERT(nth_arena_alloc(&a, 33, 1) != NULL);
    nth_usize saved = a.offset;

    for (nth_usize k = 0; k < 16; k++) {
        NTH_TEST_ASSERT(nth_arena_alloc(&a, NTH_USIZE_MAX - k, 1) == NULL);
        NTH_TEST_ASSERT(a.offset == saved);
        NTH_TEST_ASSERT(nth_arena_alloc(&a, NTH_USIZE_MAX - k, 64) == NULL);
        NTH_TEST_ASSERT(a.offset == saved);
    }

    NTH_TEST_ASSERT(nth_arena_alloc(&a, 8, 1) != NULL);

    nth_teardown_arena(&a);
    return NTH_TRUE;
}

static nth_b8 test_align_larger_than_span(void) {
    NthArena a;
    NthSpan  s = span_of(64, 4096);
    nth_setup_arena(&a, s);

    NTH_TEST_ASSERT(nth_arena_alloc(&a, 1, 1) == s.base);
    NTH_TEST_ASSERT(nth_arena_alloc(&a, 1, 4096) == NULL);
    NTH_TEST_ASSERT(a.offset == 1);

    nth_teardown_arena(&a);
    return NTH_TRUE;
}

static nth_b8 test_padding_consumes_tail(void) {
    NthArena a;
    nth_setup_arena(&a, span_of(128, 128));

    NTH_TEST_ASSERT(nth_arena_alloc(&a, 65, 1) != NULL);
    NTH_TEST_ASSERT(nth_arena_alloc(&a, 64, 64) == NULL);
    NTH_TEST_ASSERT(a.offset == 65);
    NTH_TEST_ASSERT(nth_arena_alloc(&a, 63, 1) != NULL);
    NTH_TEST_ASSERT(a.offset == 128);

    nth_teardown_arena(&a);
    return NTH_TRUE;
}

static nth_b8 test_mark_restore(void) {
    NthArena a;
    NthSpan  s = span_of(256, 64);
    nth_setup_arena(&a, s);

    nth_arena_alloc(&a, 32, 1);
    NthArenaMark m = nth_arena_mark(&a);

    nth_arena_alloc(&a, 64, 1);
    NTH_TEST_ASSERT(nth_arena_restore(&a, m));

    void *p = nth_arena_alloc(&a, 8, 1);
    NTH_TEST_ASSERT((nth_uptr)p - (nth_uptr)s.base == 32);

    nth_teardown_arena(&a);
    return NTH_TRUE;
}

static nth_b8 test_restore_idempotent(void) {
    NthArena a;
    nth_setup_arena(&a, span_of(256, 64));

    nth_arena_alloc(&a, 48, 1);
    NthArenaMark m = nth_arena_mark(&a);
    nth_arena_alloc(&a, 64, 1);

    for (int i = 0; i < 4; i++) {
        NTH_TEST_ASSERT(nth_arena_restore(&a, m));
        NTH_TEST_ASSERT(a.offset == 48);
    }

    nth_teardown_arena(&a);
    return NTH_TRUE;
}

static nth_b8 test_restore_forward_rejected(void) {
    NthArena a;
    nth_setup_arena(&a, span_of(256, 64));

    NthArenaMark m0 = nth_arena_mark(&a);
    nth_arena_alloc(&a, 32, 1);
    NthArenaMark m1 = nth_arena_mark(&a);

    NTH_TEST_ASSERT(nth_arena_restore(&a, m0));
    NTH_TEST_ASSERT(a.offset == 0);

    NTH_TEST_ASSERT(!nth_arena_restore(&a, m1));
    NTH_TEST_ASSERT(a.offset == 0);

    nth_teardown_arena(&a);
    return NTH_TRUE;
}

static nth_b8 test_restore_nested_deep(void) {
    NthArena a;
    nth_setup_arena(&a, span_of(1024, 64));

    NthArenaMark m[32];
    nth_usize    off[32];

    for (nth_usize i = 0; i < 32; i++) {
        m[i]   = nth_arena_mark(&a);
        off[i] = a.offset;
        NTH_TEST_ASSERT(nth_arena_alloc(&a, 8, 8) != NULL);
    }

    for (nth_usize i = 32; i-- > 0; ) {
        NTH_TEST_ASSERT(nth_arena_restore(&a, m[i]));
        NTH_TEST_ASSERT(a.offset == off[i]);
    }

    NTH_TEST_ASSERT(a.offset == 0);

    nth_teardown_arena(&a);
    return NTH_TRUE;
}

static nth_b8 test_mark_fresh_and_clean(void) {
    NthArena a;
    nth_setup_arena(&a, span_of(256, 64));

    NthArenaMark m0 = nth_arena_mark(&a);
    NTH_TEST_ASSERT(m0.v == 0);

    nth_arena_alloc(&a, 64, 1);
    NthArenaMark m1 = nth_arena_mark(&a);

    nth_arena_clean(&a);
    NTH_TEST_ASSERT(a.offset == 0);
    NTH_TEST_ASSERT(!nth_arena_restore(&a, m1));
    NTH_TEST_ASSERT(a.offset == 0);
    NTH_TEST_ASSERT(nth_arena_restore(&a, m0));

    nth_teardown_arena(&a);
    return NTH_TRUE;
}

static nth_b8 test_clean_reuses_base(void) {
    NthArena a;
    NthSpan  s = span_of(256, 64);
    nth_setup_arena(&a, s);

    NTH_TEST_ASSERT(nth_arena_alloc(&a, 128, 1) == s.base);
    nth_arena_clean(&a);
    NTH_TEST_ASSERT(nth_arena_alloc(&a, 8, 1) == s.base);

    nth_teardown_arena(&a);
    return NTH_TRUE;
}

static nth_b8 test_teardown_zeroes(void) {
    NthArena a;
    nth_setup_arena(&a, span_of(256, 64));
    nth_arena_alloc(&a, 64, 1);

    nth_teardown_arena(&a);

    NTH_TEST_ASSERT(a.span.base == NULL);
    NTH_TEST_ASSERT(a.span.size == 0);
    NTH_TEST_ASSERT(a.offset == 0);

    return NTH_TRUE;
}

static unsigned g_rs = 2463534242u;
static unsigned rnd(void) {
    g_rs = g_rs * 1664525u + 1013904223u;
    return g_rs >> 8;
}

static nth_b8 test_stress_model(void) {
    NthArena a;
    NthSpan  s = span_of(512, 64);
    nth_setup_arena(&a, s);

    nth_u8   *blk[MAX_BLK];
    nth_usize len[MAX_BLK];
    nth_usize live = 0;

    NthArenaMark mk[32];
    nth_usize    mlive[32];
    nth_usize    depth = 0;

    nth_usize ok = 0, oom = 0;

    for (nth_usize it = 0; it < STRESS_IT; it++) {
        unsigned op = rnd() % 100;

        if (op < 68 && live < MAX_BLK) {
            nth_usize sz = 1 + rnd() % 96;
            nth_usize al = (nth_usize)1 << (rnd() % 7);

            nth_u8 *p = nth_arena_alloc(&a, sz, al);
            if (p == NULL) { oom++; continue; }
            ok++;

            NTH_TEST_ASSERT(((nth_uptr)p & (nth_uptr)(al - 1)) == 0);
            NTH_TEST_ASSERT((nth_uptr)p >= (nth_uptr)s.base);
            NTH_TEST_ASSERT((nth_uptr)p + sz <= (nth_uptr)s.base + s.size);

            for (nth_usize i = 0; i < live; i++)
                NTH_TEST_ASSERT(p + sz <= blk[i] || blk[i] + len[i] <= p);

            memset(p, 0xA5, sz);
            blk[live] = p;
            len[live] = sz;
            live++;
        } else if (op < 80 && depth < 32) {
            mk[depth]    = nth_arena_mark(&a);
            mlive[depth] = live;
            depth++;
        } else if (op < 92 && depth > 0) {
            depth--;
            NTH_TEST_ASSERT(nth_arena_restore(&a, mk[depth]));
            live = mlive[depth];
        } else {
            nth_arena_clean(&a);
            live  = 0;
            depth = 0;
        }
    }

    NTH_TEST_ASSERT(ok > 1000);
    NTH_TEST_ASSERT(oom > 100);

    nth_teardown_arena(&a);
    return NTH_TRUE;
}


int main(void) {
    NthTest tests[] = {
        { "alloc/basic",             test_alloc_basic            },
        { "alloc/alignment_all",     test_alignment_all          },
        { "alloc/no_overlap",        test_no_overlap_patterned   },
        { "alloc/exact_fill",        test_exact_fill             },
        { "alloc/boundary_sizes",    test_boundary_sizes         },
        { "overflow/size_max",       test_overflow_size_max      },
        { "overflow/after_partial",  test_overflow_after_partial },
        { "align/larger_than_span",  test_align_larger_than_span },
        { "align/pad_consumes_tail", test_padding_consumes_tail  },
        { "mark/restore",            test_mark_restore           },
        { "mark/restore_idempotent", test_restore_idempotent     },
        { "mark/forward_rejected",   test_restore_forward_rejected },
        { "mark/nested_deep",        test_restore_nested_deep    },
        { "mark/fresh_and_clean",    test_mark_fresh_and_clean   },
        { "clean/reuses_base",       test_clean_reuses_base      },
        { "teardown/zeroes",         test_teardown_zeroes        },
        { "stress/model",            test_stress_model           },
    };

    return NTH_RUN_TESTS(tests);
}
