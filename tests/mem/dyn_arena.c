#include "nth_test.h"

#include <narthex/mem/dyn_arena.h>
#include <narthex/inl/align.h>

#include <string.h>

#define POOL      16384
#define MAX_BLK   256
#define STRESS_IT 120000

static nth_u8 g_pool[POOL];

static NthSpan chunk(nth_usize off, nth_usize size, nth_usize align) {
    nth_uptr p = nth_align_up((nth_uptr)g_pool + off, align);
    return (NthSpan){ (nth_u8 *)p, size };
}

static nth_b8 holds(NthDynArena *a, const nth_u8 *base) {
    for (nth_usize i = 0; i < a->span_count; i++)
        if (a->spans[i].base == base)
            return NTH_TRUE;
    return NTH_FALSE;
}


static nth_b8 test_alloc_basic(void) {
    NthDynArena a;
    NthSpan s = chunk(0, 256, 64);
    NTH_TEST_ASSERT(nth_setup_dyn_arena(&a, s));

    void *p = nth_dyn_arena_alloc(&a, 16, 1);
    NTH_TEST_ASSERT(p == s.base);
    NTH_TEST_ASSERT(a.offset == 16);

    nth_teardown_dyn_arena(&a);
    return NTH_TRUE;
}

static nth_b8 test_spill_to_next_span(void) {
    NthDynArena a;
    NthSpan s0 = chunk(0,    64, 64);
    NthSpan s1 = chunk(4096, 64, 64);

    NTH_TEST_ASSERT(nth_setup_dyn_arena(&a, s0));
    NTH_TEST_ASSERT(nth_dyn_arena_grow(&a, s1));

    NTH_TEST_ASSERT(nth_dyn_arena_alloc(&a, 64, 1) == s0.base);

    nth_u8 *p = nth_dyn_arena_alloc(&a, 8, 1);
    NTH_TEST_ASSERT(p >= s1.base);
    NTH_TEST_ASSERT(p + 8 <= s1.base + s1.size);
    NTH_TEST_ASSERT(a.span_idx == 1);

    nth_teardown_dyn_arena(&a);
    return NTH_TRUE;
}

static nth_b8 test_search_skips_small_span(void) {
    NthDynArena a;
    NthSpan s0 = chunk(0,     64, 64);
    NthSpan s1 = chunk(2048,  16, 64);
    NthSpan s2 = chunk(4096, 256, 64);

    NTH_TEST_ASSERT(nth_setup_dyn_arena(&a, s0));
    NTH_TEST_ASSERT(nth_dyn_arena_grow(&a, s1));
    NTH_TEST_ASSERT(nth_dyn_arena_grow(&a, s2));

    NTH_TEST_ASSERT(nth_dyn_arena_alloc(&a, 64, 1) == s0.base);

    nth_u8 *p = nth_dyn_arena_alloc(&a, 128, 1);
    NTH_TEST_ASSERT(p != NULL);
    NTH_TEST_ASSERT(p >= s2.base);
    NTH_TEST_ASSERT(p + 128 <= s2.base + s2.size);

    NTH_TEST_ASSERT(a.span_count == 3);
    NTH_TEST_ASSERT(holds(&a, s0.base));
    NTH_TEST_ASSERT(holds(&a, s1.base));
    NTH_TEST_ASSERT(holds(&a, s2.base));

    nth_teardown_dyn_arena(&a);
    return NTH_TRUE;
}

static nth_b8 test_grow_past_initial_capacity(void) {
    NthDynArena a;
    NTH_TEST_ASSERT(nth_setup_dyn_arena(&a, chunk(0, 32, 32)));

    for (nth_usize i = 1; i < 12; i++)
        NTH_TEST_ASSERT(nth_dyn_arena_grow(&a, chunk(i * 512, 32, 32)));

    NTH_TEST_ASSERT(a.span_count == 12);
    NTH_TEST_ASSERT(a.span_capacity >= 12);

    for (nth_usize i = 0; i < 12; i++)
        NTH_TEST_ASSERT(a.spans[i].base != NULL);

    nth_teardown_dyn_arena(&a);
    return NTH_TRUE;
}

static nth_b8 test_shrink_refuses_live_span(void) {
    NthDynArena a;
    NthSpan s0 = chunk(0,    64, 64);
    NthSpan s1 = chunk(2048, 64, 64);

    NTH_TEST_ASSERT(nth_setup_dyn_arena(&a, s0));
    NTH_TEST_ASSERT(nth_dyn_arena_grow(&a, s1));

    NTH_TEST_ASSERT(nth_dyn_arena_alloc(&a, 64, 1) != NULL);
    NTH_TEST_ASSERT(nth_dyn_arena_alloc(&a, 32, 1) != NULL);
    NTH_TEST_ASSERT(a.span_idx == 1);

    NTH_TEST_ASSERT(nth_dyn_arena_shrink(&a).base == NULL);
    NTH_TEST_ASSERT(a.span_count == 2);

    nth_teardown_dyn_arena(&a);
    return NTH_TRUE;
}

static nth_b8 test_shrink_full_drain(void) {
    NthDynArena a;
    NTH_TEST_ASSERT(nth_setup_dyn_arena(&a, chunk(0, 64, 64)));
    NTH_TEST_ASSERT(nth_dyn_arena_grow(&a, chunk(2048, 64, 64)));
    NTH_TEST_ASSERT(nth_dyn_arena_grow(&a, chunk(4096, 64, 64)));

    NTH_TEST_ASSERT(nth_dyn_arena_alloc(&a, 64, 1) != NULL);
    NTH_TEST_ASSERT(nth_dyn_arena_alloc(&a, 32, 1) != NULL);

    nth_dyn_arena_clean(&a);

    nth_usize removed = 0;
    while (nth_dyn_arena_shrink(&a).base != NULL)
        removed++;

    NTH_TEST_ASSERT(removed == 3);
    NTH_TEST_ASSERT(a.span_count == 0);
    NTH_TEST_ASSERT(nth_dyn_arena_alloc(&a, 8, 1) == NULL);
    NTH_TEST_ASSERT(nth_dyn_arena_shrink(&a).base == NULL);

    nth_teardown_dyn_arena(&a);
    return NTH_TRUE;
}

static nth_b8 test_shrink_returns_last_span(void) {
    NthDynArena a;
    NthSpan s0 = chunk(0,    64, 64);
    NthSpan s1 = chunk(2048, 32, 64);

    NTH_TEST_ASSERT(nth_setup_dyn_arena(&a, s0));
    NTH_TEST_ASSERT(nth_dyn_arena_grow(&a, s1));

    NthSpan got = nth_dyn_arena_shrink(&a);
    NTH_TEST_ASSERT(got.base == s1.base);
    NTH_TEST_ASSERT(got.size == s1.size);
    NTH_TEST_ASSERT(a.span_count == 1);

    nth_teardown_dyn_arena(&a);
    return NTH_TRUE;
}

static nth_b8 test_alignment_all(void) {
    for (nth_usize sh = 0; sh <= 10; sh++) {
        nth_usize align = (nth_usize)1 << sh;

        for (nth_usize skew = 0; skew < 8; skew++) {
            NthDynArena a;
            NTH_TEST_ASSERT(nth_setup_dyn_arena(&a, chunk(0, 4096, 4096)));

            if (skew != 0)
                NTH_TEST_ASSERT(nth_dyn_arena_alloc(&a, skew, 1) != NULL);

            void *p = nth_dyn_arena_alloc(&a, 8, align);
            NTH_TEST_ASSERT(p != NULL);
            NTH_TEST_ASSERT(((nth_uptr)p & (nth_uptr)(align - 1)) == 0);

            nth_teardown_dyn_arena(&a);
        }
    }
    return NTH_TRUE;
}

static nth_b8 test_overflow_size_max(void) {
    const nth_usize aligns[] = { 1, 8, 64, 4096 };

    for (nth_usize i = 0; i < 4; i++) {
        for (nth_usize k = 0; k < 16; k++) {
            NthDynArena a;
            NTH_TEST_ASSERT(nth_setup_dyn_arena(&a, chunk(0, 64, 4096)));
            NTH_TEST_ASSERT(nth_dyn_arena_grow(&a, chunk(4096, 128, 64)));

            NTH_TEST_ASSERT(nth_dyn_arena_alloc(&a, NTH_USIZE_MAX - k, aligns[i]) == NULL);
            NTH_TEST_ASSERT(a.offset == 0);
            NTH_TEST_ASSERT(a.span_idx == 0);

            nth_teardown_dyn_arena(&a);
        }
    }
    return NTH_TRUE;
}

static nth_b8 test_exact_fill_across_spans(void) {
    NthDynArena a;
    NthSpan s0 = chunk(0,    64, 64);
    NthSpan s1 = chunk(2048, 64, 64);

    NTH_TEST_ASSERT(nth_setup_dyn_arena(&a, s0));
    NTH_TEST_ASSERT(nth_dyn_arena_grow(&a, s1));

    NTH_TEST_ASSERT(nth_dyn_arena_alloc(&a, 64, 1) == s0.base);
    NTH_TEST_ASSERT(nth_dyn_arena_alloc(&a, 64, 1) == s1.base);
    NTH_TEST_ASSERT(nth_dyn_arena_alloc(&a, 1, 1) == NULL);

    nth_teardown_dyn_arena(&a);
    return NTH_TRUE;
}

static nth_b8 test_mark_restore_across_spans(void) {
    NthDynArena a;
    NthSpan s0 = chunk(0,    64, 64);
    NthSpan s1 = chunk(2048, 64, 64);

    NTH_TEST_ASSERT(nth_setup_dyn_arena(&a, s0));
    NTH_TEST_ASSERT(nth_dyn_arena_grow(&a, s1));

    NTH_TEST_ASSERT(nth_dyn_arena_alloc(&a, 64, 1) != NULL);
    NTH_TEST_ASSERT(nth_dyn_arena_alloc(&a, 16, 1) != NULL);

    NthDynArenaMark m = nth_dyn_arena_mark(&a);
    NTH_TEST_ASSERT(a.span_idx == 1);
    NTH_TEST_ASSERT(a.offset == 16);

    NTH_TEST_ASSERT(nth_dyn_arena_alloc(&a, 16, 1) != NULL);
    NTH_TEST_ASSERT(nth_dyn_arena_restore(&a, m));
    NTH_TEST_ASSERT(a.span_idx == 1);
    NTH_TEST_ASSERT(a.offset == 16);

    nth_teardown_dyn_arena(&a);
    return NTH_TRUE;
}

static nth_b8 test_restore_forward_rejected(void) {
    NthDynArena a;
    NTH_TEST_ASSERT(nth_setup_dyn_arena(&a, chunk(0, 64, 64)));
    NTH_TEST_ASSERT(nth_dyn_arena_grow(&a, chunk(2048, 64, 64)));

    NTH_TEST_ASSERT(nth_dyn_arena_alloc(&a, 32, 1) != NULL);
    NthDynArenaMark m0 = nth_dyn_arena_mark(&a);

    NTH_TEST_ASSERT(nth_dyn_arena_alloc(&a, 32, 1) != NULL);
    NTH_TEST_ASSERT(nth_dyn_arena_alloc(&a, 16, 1) != NULL);
    NthDynArenaMark m1 = nth_dyn_arena_mark(&a);

    NTH_TEST_ASSERT(nth_dyn_arena_restore(&a, m0));
    NTH_TEST_ASSERT(a.span_idx == 0);
    NTH_TEST_ASSERT(a.offset == 32);

    NTH_TEST_ASSERT(!nth_dyn_arena_restore(&a, m1));
    NTH_TEST_ASSERT(a.span_idx == 0);
    NTH_TEST_ASSERT(a.offset == 32);

    nth_teardown_dyn_arena(&a);
    return NTH_TRUE;
}

static nth_b8 test_restore_rejects_out_of_range(void) {
    NthDynArena a;
    NTH_TEST_ASSERT(nth_setup_dyn_arena(&a, chunk(0, 64, 64)));
    NTH_TEST_ASSERT(nth_dyn_arena_grow(&a, chunk(2048, 64, 64)));
    NTH_TEST_ASSERT(nth_dyn_arena_alloc(&a, 32, 1) != NULL);

    nth_usize idx = a.span_idx;
    nth_usize off = a.offset;

    NthDynArenaMark far_idx = { ((nth_u64)9 << 48) };
    NTH_TEST_ASSERT(!nth_dyn_arena_restore(&a, far_idx));
    NTH_TEST_ASSERT(a.span_idx == idx && a.offset == off);

    NthDynArenaMark far_off = { (nth_u64)9999 };
    NTH_TEST_ASSERT(!nth_dyn_arena_restore(&a, far_off));
    NTH_TEST_ASSERT(a.span_idx == idx && a.offset == off);

    NthDynArenaMark self = nth_dyn_arena_mark(&a);
    NTH_TEST_ASSERT(nth_dyn_arena_restore(&a, self));
    NTH_TEST_ASSERT(a.span_idx == idx && a.offset == off);

    nth_teardown_dyn_arena(&a);
    return NTH_TRUE;
}

static nth_b8 test_mark_packs_idx_and_offset(void) {
    NthDynArena a;
    NTH_TEST_ASSERT(nth_setup_dyn_arena(&a, chunk(0, 64, 64)));
    NTH_TEST_ASSERT(nth_dyn_arena_grow(&a, chunk(2048, 64, 64)));

    NthDynArenaMark m0 = nth_dyn_arena_mark(&a);
    NTH_TEST_ASSERT(m0.v == 0);

    NTH_TEST_ASSERT(nth_dyn_arena_alloc(&a, 64, 1) != NULL);
    NTH_TEST_ASSERT(nth_dyn_arena_alloc(&a, 24, 1) != NULL);

    NthDynArenaMark m1 = nth_dyn_arena_mark(&a);
    NTH_TEST_ASSERT((m1.v >> 48) == 1);
    NTH_TEST_ASSERT((m1.v & 0xFFFFFFFFFFFFu) == 24);

    nth_teardown_dyn_arena(&a);
    return NTH_TRUE;
}

static nth_b8 test_clean_and_teardown(void) {
    NthDynArena a;
    NthSpan s0 = chunk(0, 64, 64);
    NTH_TEST_ASSERT(nth_setup_dyn_arena(&a, s0));
    NTH_TEST_ASSERT(nth_dyn_arena_grow(&a, chunk(2048, 64, 64)));

    NTH_TEST_ASSERT(nth_dyn_arena_alloc(&a, 64, 1) != NULL);
    NTH_TEST_ASSERT(nth_dyn_arena_alloc(&a, 32, 1) != NULL);

    nth_dyn_arena_clean(&a);
    NTH_TEST_ASSERT(a.span_idx == 0);
    NTH_TEST_ASSERT(a.offset == 0);
    NTH_TEST_ASSERT(nth_dyn_arena_alloc(&a, 8, 1) == s0.base);

    nth_teardown_dyn_arena(&a);
    NTH_TEST_ASSERT(a.spans == NULL);
    NTH_TEST_ASSERT(a.span_count == 0);
    NTH_TEST_ASSERT(a.span_capacity == 0);
    NTH_TEST_ASSERT(a.span_idx == 0);
    NTH_TEST_ASSERT(a.offset == 0);

    return NTH_TRUE;
}

static unsigned g_rs = 88675123u;
static unsigned rnd(void) {
    g_rs = g_rs * 1664525u + 1013904223u;
    return g_rs >> 8;
}

static nth_b8 in_any_span(NthDynArena *a, const nth_u8 *p, nth_usize n) {
    for (nth_usize i = 0; i < a->span_count; i++)
        if (p >= a->spans[i].base && p + n <= a->spans[i].base + a->spans[i].size)
            return NTH_TRUE;
    return NTH_FALSE;
}

static nth_b8 test_stress_model(void) {
    NthDynArena a;
    NTH_TEST_ASSERT(nth_setup_dyn_arena(&a, chunk(0, 96, 64)));
    NTH_TEST_ASSERT(nth_dyn_arena_grow(&a, chunk(2048,  48, 64)));
    NTH_TEST_ASSERT(nth_dyn_arena_grow(&a, chunk(4096, 128, 64)));
    NTH_TEST_ASSERT(nth_dyn_arena_grow(&a, chunk(8192,  64, 64)));

    nth_u8   *blk[MAX_BLK];
    nth_usize len[MAX_BLK];
    nth_usize live = 0;

    NthDynArenaMark mk[32];
    nth_usize       mlive[32];
    nth_usize       depth = 0;

    nth_usize ok = 0, oom = 0;

    for (nth_usize it = 0; it < STRESS_IT; it++) {
        unsigned op = rnd() % 100;

        if (op < 68 && live < MAX_BLK) {
            nth_usize sz = 1 + rnd() % 96;
            nth_usize al = (nth_usize)1 << (rnd() % 7);

            nth_u8 *p = nth_dyn_arena_alloc(&a, sz, al);
            if (p == NULL) { oom++; continue; }
            ok++;

            NTH_TEST_ASSERT(((nth_uptr)p & (nth_uptr)(al - 1)) == 0);
            NTH_TEST_ASSERT(in_any_span(&a, p, sz));

            for (nth_usize i = 0; i < live; i++)
                NTH_TEST_ASSERT(p + sz <= blk[i] || blk[i] + len[i] <= p);

            memset(p, 0x5A, sz);
            blk[live] = p;
            len[live] = sz;
            live++;
        } else if (op < 80 && depth < 32) {
            mk[depth]    = nth_dyn_arena_mark(&a);
            mlive[depth] = live;
            depth++;
        } else if (op < 92 && depth > 0) {
            depth--;
            NTH_TEST_ASSERT(nth_dyn_arena_restore(&a, mk[depth]));
            live = mlive[depth];
        } else {
            nth_dyn_arena_clean(&a);
            live  = 0;
            depth = 0;
        }

        NTH_TEST_ASSERT(a.span_idx < a.span_count);
        NTH_TEST_ASSERT(a.offset <= a.spans[a.span_idx].size);
    }

    NTH_TEST_ASSERT(ok > 1000);
    NTH_TEST_ASSERT(oom > 100);

    nth_teardown_dyn_arena(&a);
    return NTH_TRUE;
}


int main(void) {
    NthTest tests[] = {
        { "alloc/basic",             test_alloc_basic                },
        { "alloc/spill",             test_spill_to_next_span         },
        { "alloc/search_skips_small",test_search_skips_small_span    },
        { "alloc/alignment_all",     test_alignment_all              },
        { "alloc/exact_fill_spans",  test_exact_fill_across_spans    },
        { "overflow/size_max",       test_overflow_size_max          },
        { "grow/past_capacity",      test_grow_past_initial_capacity },
        { "shrink/refuses_live",     test_shrink_refuses_live_span   },
        { "shrink/full_drain",       test_shrink_full_drain          },
        { "shrink/returns_last",     test_shrink_returns_last_span   },
        { "mark/across_spans",       test_mark_restore_across_spans  },
        { "mark/forward_rejected",   test_restore_forward_rejected   },
        { "mark/out_of_range",       test_restore_rejects_out_of_range },
        { "mark/packing",            test_mark_packs_idx_and_offset  },
        { "clean_and_teardown",      test_clean_and_teardown         },
        { "stress/model",            test_stress_model               },
    };

    return NTH_RUN_TESTS(tests);
}
