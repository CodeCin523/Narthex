#include "nth_test.h"

#include <narthex/mem/free_list.h>
#include <narthex/inl/align.h>

#include <stdalign.h>
#include <string.h>


#define POOL      16384
#define MAX_BLK   128
#define STRESS_IT 120000

static nth_u8 g_pool[POOL];


static NthSpan chunk(nth_usize off, nth_usize size, nth_usize align) {
    nth_uptr p = nth_align_up((nth_uptr)g_pool + off, align);
    return (NthSpan){ (nth_u8 *)p, size };
}


static nth_b8 in_span(NthSpan span, const nth_u8 *p, nth_usize size) {
    return p >= span.base && p + size <= span.base + span.size;
}


static nth_b8 in_any_span(NthFreeList *list, const nth_u8 *p, nth_usize size) {
    for (nth_usize i = 0; i < list->span_count; i++)
        if (in_span(list->spans[i], p, size))
            return NTH_TRUE;

    return NTH_FALSE;
}


/* ================================================================================ */
/*  BASIC                                                                           */
/* ================================================================================ */

static nth_b8 test_alloc_basic(void) {
    NthFreeList list;
    NthSpan s = chunk(0, 4096, 64);

    NTH_TEST_ASSERT(nth_setup_free_list(&list, s) == NTH_RESULT_OK);

    void *p = nth_free_list_alloc(&list, 16);

    NTH_TEST_ASSERT(p != NULL);
    NTH_TEST_ASSERT(in_span(s, p, 16));
    NTH_TEST_ASSERT(list.p_used != NULL);
    NTH_TEST_ASSERT(list.p_free != NULL);

    nth_free_list_free(&list, p);

    NTH_TEST_ASSERT(list.p_used == NULL);
    NTH_TEST_ASSERT(list.p_free != NULL);

    nth_teardown_free_list(&list);

    return NTH_TRUE;
}


static nth_b8 test_alloc_multiple(void) {
    NthFreeList list;
    NthSpan s = chunk(0, 4096, 64);

    NTH_TEST_ASSERT(nth_setup_free_list(&list, s) == NTH_RESULT_OK);

    void *p0 = nth_free_list_alloc(&list, 1);
    void *p1 = nth_free_list_alloc(&list, 32);
    void *p2 = nth_free_list_alloc(&list, 64);
    void *p3 = nth_free_list_alloc(&list, 127);

    NTH_TEST_ASSERT(p0 != NULL);
    NTH_TEST_ASSERT(p1 != NULL);
    NTH_TEST_ASSERT(p2 != NULL);
    NTH_TEST_ASSERT(p3 != NULL);

    NTH_TEST_ASSERT(in_span(s, p0, 1));
    NTH_TEST_ASSERT(in_span(s, p1, 32));
    NTH_TEST_ASSERT(in_span(s, p2, 64));
    NTH_TEST_ASSERT(in_span(s, p3, 127));

    NTH_TEST_ASSERT(p0 != p1);
    NTH_TEST_ASSERT(p0 != p2);
    NTH_TEST_ASSERT(p0 != p3);
    NTH_TEST_ASSERT(p1 != p2);
    NTH_TEST_ASSERT(p1 != p3);
    NTH_TEST_ASSERT(p2 != p3);

    nth_free_list_free(&list, p0);
    nth_free_list_free(&list, p1);
    nth_free_list_free(&list, p2);
    nth_free_list_free(&list, p3);

    nth_teardown_free_list(&list);

    return NTH_TRUE;
}


static nth_b8 test_alloc_alignment(void) {
    NthFreeList list;
    NthSpan s = chunk(3, 4096, 64);

    NTH_TEST_ASSERT(nth_setup_free_list(&list, s) == NTH_RESULT_OK);

    void *p0 = nth_free_list_alloc(&list, 1);
    void *p1 = nth_free_list_alloc(&list, 17);
    void *p2 = nth_free_list_alloc(&list, 64);

    NTH_TEST_ASSERT(p0 != NULL);
    NTH_TEST_ASSERT(p1 != NULL);
    NTH_TEST_ASSERT(p2 != NULL);

    NTH_TEST_ASSERT(((nth_uptr)p0 % alignof(max_align_t)) == 0);
    NTH_TEST_ASSERT(((nth_uptr)p1 % alignof(max_align_t)) == 0);
    NTH_TEST_ASSERT(((nth_uptr)p2 % alignof(max_align_t)) == 0);

    nth_free_list_free(&list, p0);
    nth_free_list_free(&list, p1);
    nth_free_list_free(&list, p2);

    nth_teardown_free_list(&list);

    return NTH_TRUE;
}


static nth_b8 test_alloc_granularity(void) {
    NthFreeList list;
    NthSpan s = chunk(0, 4096, 64);

    NTH_TEST_ASSERT(nth_setup_free_list(&list, s) == NTH_RESULT_OK);

    void *p0 = nth_free_list_alloc(&list, 1);
    void *p1 = nth_free_list_alloc(&list, 32);
    void *p2 = nth_free_list_alloc(&list, 33);

    NTH_TEST_ASSERT(p0 != NULL);
    NTH_TEST_ASSERT(p1 != NULL);
    NTH_TEST_ASSERT(p2 != NULL);

    /*
     * 1 byte  -> 16 bytes data + 16 bytes meta = 32 bytes.
     * 32 bytes -> 32 bytes data + 16 bytes meta = 48 bytes.
     * 33 bytes -> 48 bytes data + 16 bytes meta = 64 bytes.
     */
    NTH_TEST_ASSERT((nth_u8 *)p1 - (nth_u8 *)p0 == 32);
    NTH_TEST_ASSERT((nth_u8 *)p2 - (nth_u8 *)p1 == 48);

    nth_free_list_free(&list, p0);
    nth_free_list_free(&list, p1);
    nth_free_list_free(&list, p2);

    nth_teardown_free_list(&list);

    return NTH_TRUE;
}


/* ================================================================================ */
/*  FREE / REUSE                                                                    */
/* ================================================================================ */

static nth_b8 test_free_reuses_block(void) {
    NthFreeList list;
    NthSpan s = chunk(0, 4096, 64);

    NTH_TEST_ASSERT(nth_setup_free_list(&list, s) == NTH_RESULT_OK);

    void *p0 = nth_free_list_alloc(&list, 64);

    NTH_TEST_ASSERT(p0 != NULL);

    nth_free_list_free(&list, p0);

    void *p1 = nth_free_list_alloc(&list, 64);

    NTH_TEST_ASSERT(p1 == p0);

    nth_free_list_free(&list, p1);
    nth_teardown_free_list(&list);

    return NTH_TRUE;
}


static nth_b8 test_free_reuses_fragment(void) {
    NthFreeList list;
    NthSpan s = chunk(0, 4096, 64);

    NTH_TEST_ASSERT(nth_setup_free_list(&list, s) == NTH_RESULT_OK);

    void *p0 = nth_free_list_alloc(&list, 32);
    void *p1 = nth_free_list_alloc(&list, 32);
    void *p2 = nth_free_list_alloc(&list, 32);

    NTH_TEST_ASSERT(p0 != NULL);
    NTH_TEST_ASSERT(p1 != NULL);
    NTH_TEST_ASSERT(p2 != NULL);

    nth_free_list_free(&list, p1);

    void *p3 = nth_free_list_alloc(&list, 32);

    NTH_TEST_ASSERT(p3 == p1);

    nth_free_list_free(&list, p0);
    nth_free_list_free(&list, p2);
    nth_free_list_free(&list, p3);

    nth_teardown_free_list(&list);

    return NTH_TRUE;
}


/* ================================================================================ */
/*  COALESCING                                                                      */
/* ================================================================================ */

static nth_b8 test_coalesce_forward(void) {
    NthFreeList list;
    NthSpan s = chunk(0, 4096, 64);

    NTH_TEST_ASSERT(nth_setup_free_list(&list, s) == NTH_RESULT_OK);

    void *p0 = nth_free_list_alloc(&list, 32);
    void *p1 = nth_free_list_alloc(&list, 32);
    void *p2 = nth_free_list_alloc(&list, 32);

    NTH_TEST_ASSERT(p0 != NULL);
    NTH_TEST_ASSERT(p1 != NULL);
    NTH_TEST_ASSERT(p2 != NULL);

    nth_free_list_free(&list, p1);
    nth_free_list_free(&list, p0);

    void *p3 = nth_free_list_alloc(&list, 64);

    NTH_TEST_ASSERT(p3 == p0);

    nth_free_list_free(&list, p2);
    nth_free_list_free(&list, p3);

    nth_teardown_free_list(&list);

    return NTH_TRUE;
}


static nth_b8 test_coalesce_backward(void) {
    NthFreeList list;
    NthSpan s = chunk(0, 4096, 64);

    NTH_TEST_ASSERT(nth_setup_free_list(&list, s) == NTH_RESULT_OK);

    void *p0 = nth_free_list_alloc(&list, 32);
    void *p1 = nth_free_list_alloc(&list, 32);
    void *p2 = nth_free_list_alloc(&list, 32);

    NTH_TEST_ASSERT(p0 != NULL);
    NTH_TEST_ASSERT(p1 != NULL);
    NTH_TEST_ASSERT(p2 != NULL);

    nth_free_list_free(&list, p0);
    nth_free_list_free(&list, p1);

    void *p3 = nth_free_list_alloc(&list, 64);

    NTH_TEST_ASSERT(p3 == p0);

    nth_free_list_free(&list, p2);
    nth_free_list_free(&list, p3);

    nth_teardown_free_list(&list);

    return NTH_TRUE;
}


static nth_b8 test_coalesce_both(void) {
    NthFreeList list;
    NthSpan s = chunk(0, 4096, 64);

    NTH_TEST_ASSERT(nth_setup_free_list(&list, s) == NTH_RESULT_OK);

    void *p0 = nth_free_list_alloc(&list, 32);
    void *p1 = nth_free_list_alloc(&list, 32);
    void *p2 = nth_free_list_alloc(&list, 32);

    NTH_TEST_ASSERT(p0 != NULL);
    NTH_TEST_ASSERT(p1 != NULL);
    NTH_TEST_ASSERT(p2 != NULL);

    nth_free_list_free(&list, p0);
    nth_free_list_free(&list, p2);
    nth_free_list_free(&list, p1);

    void *p3 = nth_free_list_alloc(&list, 128);

    NTH_TEST_ASSERT(p3 == p0);

    nth_free_list_free(&list, p3);
    nth_teardown_free_list(&list);

    return NTH_TRUE;
}


/* ================================================================================ */
/*  CAPACITY                                                                        */
/* ================================================================================ */

static nth_b8 test_allocation_failure(void) {
    NthFreeList list;
    NthSpan s = chunk(0, 256, 64);

    NTH_TEST_ASSERT(nth_setup_free_list(&list, s) == NTH_RESULT_OK);

    void *p = nth_free_list_alloc(&list, 4096);

    NTH_TEST_ASSERT(p == NULL);
    NTH_TEST_ASSERT(list.p_used == NULL);
    NTH_TEST_ASSERT(list.p_free != NULL);

    nth_teardown_free_list(&list);

    return NTH_TRUE;
}


static nth_b8 test_exact_fill(void) {
    /*
     * A 64-byte allocation requires:
     *
     *     2 data chunks + 1 metadata chunk = 3 chunks = 96 bytes.
     *
     * 384 bytes therefore contain exactly four such allocations.
     */
    NthFreeList list;
    NthSpan s = chunk(0, 384, 64);

    NTH_TEST_ASSERT(nth_setup_free_list(&list, s) == NTH_RESULT_OK);

    void *blk[4];

    for (nth_usize i = 0; i < 4; i++) {
        blk[i] = nth_free_list_alloc(&list, 64);
        NTH_TEST_ASSERT(blk[i] != NULL);
    }

    NTH_TEST_ASSERT(nth_free_list_alloc(&list, 64) == NULL);

    for (nth_usize i = 0; i < 4; i++)
        nth_free_list_free(&list, blk[i]);

    nth_teardown_free_list(&list);

    return NTH_TRUE;
}


/* ================================================================================ */
/*  GROW                                                                            */
/* ================================================================================ */

static nth_b8 test_grow(void) {
    NthFreeList list;
    NthSpan s0 = chunk(0,    256, 64);
    NthSpan s1 = chunk(2048, 256, 64);

    NTH_TEST_ASSERT(nth_setup_free_list(&list, s0) == NTH_RESULT_OK);
    NTH_TEST_ASSERT(nth_free_list_grow(&list, s1));

    NTH_TEST_ASSERT(list.span_count == 2);
    NTH_TEST_ASSERT(list.spans[0].base == s0.base);
    NTH_TEST_ASSERT(list.spans[1].base == s1.base);

    nth_teardown_free_list(&list);

    return NTH_TRUE;
}


static nth_b8 test_grow_uses_new_span(void) {
    /*
     * The first span is intentionally too small for a 64-byte
     * allocation because the allocator needs metadata as well.
     */
    NthFreeList list;
    NthSpan s0 = chunk(0,    64, 64);
    NthSpan s1 = chunk(2048, 256, 64);

    NTH_TEST_ASSERT(nth_setup_free_list(&list, s0) == NTH_RESULT_OK);
    NTH_TEST_ASSERT(nth_free_list_grow(&list, s1));

    void *p = nth_free_list_alloc(&list, 64);

    NTH_TEST_ASSERT(p != NULL);
    NTH_TEST_ASSERT(in_span(s1, p, 64));

    nth_free_list_free(&list, p);
    nth_teardown_free_list(&list);

    return NTH_TRUE;
}


static nth_b8 test_grow_past_initial_capacity(void) {
    NthFreeList list;

    NTH_TEST_ASSERT(
        nth_setup_free_list(&list, chunk(0, 256, 64))
        == NTH_RESULT_OK
    );

    for (nth_usize i = 1; i < 12; i++)
        NTH_TEST_ASSERT(
            nth_free_list_grow(&list, chunk(i * 512, 256, 64))
        );

    NTH_TEST_ASSERT(list.span_count == 12);
    NTH_TEST_ASSERT(list.span_capacity >= 12);

    for (nth_usize i = 0; i < 12; i++)
        NTH_TEST_ASSERT(list.spans[i].base != NULL);

    nth_teardown_free_list(&list);

    return NTH_TRUE;
}


/* ================================================================================ */
/*  SHRINK                                                                          */
/* ================================================================================ */

static nth_b8 test_shrink_returns_last_span(void) {
    NthFreeList list;
    NthSpan s0 = chunk(0,    256, 64);
    NthSpan s1 = chunk(2048, 256, 64);

    NTH_TEST_ASSERT(nth_setup_free_list(&list, s0) == NTH_RESULT_OK);
    NTH_TEST_ASSERT(nth_free_list_grow(&list, s1));

    NthSpan got = nth_free_list_shrink(&list);

    NTH_TEST_ASSERT(got.base == s1.base);
    NTH_TEST_ASSERT(got.size == s1.size);
    NTH_TEST_ASSERT(list.span_count == 1);

    nth_teardown_free_list(&list);

    return NTH_TRUE;
}


static nth_b8 test_shrink_refuses_live_span(void) {
    /*
     * Make s0 too small for the allocation so the allocation is
     * guaranteed to come from the last span.
     */
    NthFreeList list;
    NthSpan s0 = chunk(0,    64, 64);
    NthSpan s1 = chunk(2048, 256, 64);

    NTH_TEST_ASSERT(nth_setup_free_list(&list, s0) == NTH_RESULT_OK);
    NTH_TEST_ASSERT(nth_free_list_grow(&list, s1));

    void *p = nth_free_list_alloc(&list, 64);

    NTH_TEST_ASSERT(p != NULL);
    NTH_TEST_ASSERT(in_span(s1, p, 64));

    NthSpan got = nth_free_list_shrink(&list);

    NTH_TEST_ASSERT(got.base == NULL);
    NTH_TEST_ASSERT(got.size == 0);
    NTH_TEST_ASSERT(list.span_count == 2);

    nth_free_list_free(&list, p);
    nth_teardown_free_list(&list);

    return NTH_TRUE;
}


static nth_b8 test_shrink_after_free(void) {
    /*
     * Again, force the allocation into the last span.
     */
    NthFreeList list;
    NthSpan s0 = chunk(0,    64, 64);
    NthSpan s1 = chunk(2048, 256, 64);

    NTH_TEST_ASSERT(nth_setup_free_list(&list, s0) == NTH_RESULT_OK);
    NTH_TEST_ASSERT(nth_free_list_grow(&list, s1));

    void *p = nth_free_list_alloc(&list, 64);

    NTH_TEST_ASSERT(p != NULL);
    NTH_TEST_ASSERT(in_span(s1, p, 64));

    nth_free_list_free(&list, p);

    NthSpan got = nth_free_list_shrink(&list);

    NTH_TEST_ASSERT(got.base == s1.base);
    NTH_TEST_ASSERT(got.size == s1.size);
    NTH_TEST_ASSERT(list.span_count == 1);

    nth_teardown_free_list(&list);

    return NTH_TRUE;
}


static nth_b8 test_shrink_empty(void) {
    NthFreeList list;
    NthSpan s = chunk(0, 256, 64);

    NTH_TEST_ASSERT(nth_setup_free_list(&list, s) == NTH_RESULT_OK);

    /*
     * The allocator always starts with one span, so shrink should
     * remove that span when nothing is live.
     */
    NthSpan got = nth_free_list_shrink(&list);

    NTH_TEST_ASSERT(got.base == s.base);
    NTH_TEST_ASSERT(got.size == s.size);
    NTH_TEST_ASSERT(list.span_count == 0);

    NTH_TEST_ASSERT(nth_free_list_shrink(&list).base == NULL);

    nth_teardown_free_list(&list);

    return NTH_TRUE;
}


/* ================================================================================ */
/*  CLEAR                                                                           */
/* ================================================================================ */

static nth_b8 test_clear(void) {
    NthFreeList list;
    NthSpan s0 = chunk(0,    512, 64);
    NthSpan s1 = chunk(2048, 512, 64);

    NTH_TEST_ASSERT(nth_setup_free_list(&list, s0) == NTH_RESULT_OK);
    NTH_TEST_ASSERT(nth_free_list_grow(&list, s1));

    void *p0 = nth_free_list_alloc(&list, 64);
    void *p1 = nth_free_list_alloc(&list, 64);
    void *p2 = nth_free_list_alloc(&list, 64);

    NTH_TEST_ASSERT(p0 != NULL);
    NTH_TEST_ASSERT(p1 != NULL);
    NTH_TEST_ASSERT(p2 != NULL);

    nth_free_list_clear(&list);

    NTH_TEST_ASSERT(list.p_used == NULL);
    NTH_TEST_ASSERT(list.p_free != NULL);

    void *p3 = nth_free_list_alloc(&list, 64);

    NTH_TEST_ASSERT(p3 != NULL);
    NTH_TEST_ASSERT(in_any_span(&list, p3, 64));

    nth_free_list_free(&list, p3);
    nth_teardown_free_list(&list);

    return NTH_TRUE;
}


/* ================================================================================ */
/*  ALLOCATOR                                                                       */
/* ================================================================================ */

static nth_b8 test_as_allocator(void) {
    NthFreeList list;
    NthSpan s = chunk(0, 4096, 64);

    NTH_TEST_ASSERT(nth_setup_free_list(&list, s) == NTH_RESULT_OK);

    NthAllocator allocator = nth_free_list_as_allocator(&list);

    NTH_TEST_ASSERT(allocator.ctx == &list);
    NTH_TEST_ASSERT(allocator.alloc != NULL);
    NTH_TEST_ASSERT(allocator.free != NULL);
    NTH_TEST_ASSERT(allocator.realloc != NULL);
    NTH_TEST_ASSERT(allocator.clear != NULL);

    void *p = allocator.alloc(allocator.ctx, 64, 1);

    NTH_TEST_ASSERT(p != NULL);
    NTH_TEST_ASSERT(in_span(s, p, 64));

    allocator.free(allocator.ctx, p);

    NTH_TEST_ASSERT(list.p_used == NULL);

    nth_teardown_free_list(&list);

    return NTH_TRUE;
}


static nth_b8 test_allocator_realloc(void) {
    NthFreeList list;
    NthSpan s = chunk(0, 4096, 64);

    NTH_TEST_ASSERT(nth_setup_free_list(&list, s) == NTH_RESULT_OK);

    NthAllocator allocator = nth_free_list_as_allocator(&list);

    nth_u8 *p = allocator.alloc(allocator.ctx, 64, 1);

    NTH_TEST_ASSERT(p != NULL);

    for (nth_usize i = 0; i < 64; i++)
        p[i] = (nth_u8)i;

    nth_u8 *q = allocator.realloc(allocator.ctx, p, 128, 1);

    NTH_TEST_ASSERT(q != NULL);

    for (nth_usize i = 0; i < 64; i++)
        NTH_TEST_ASSERT(q[i] == (nth_u8)i);

    allocator.free(allocator.ctx, q);

    nth_teardown_free_list(&list);

    return NTH_TRUE;
}


static nth_b8 test_allocator_realloc_null(void) {
    NthFreeList list;
    NthSpan s = chunk(0, 4096, 64);

    NTH_TEST_ASSERT(nth_setup_free_list(&list, s) == NTH_RESULT_OK);

    NthAllocator allocator = nth_free_list_as_allocator(&list);

    void *p = allocator.realloc(allocator.ctx, NULL, 64, 1);

    NTH_TEST_ASSERT(p != NULL);

    allocator.free(allocator.ctx, p);

    nth_teardown_free_list(&list);

    return NTH_TRUE;
}


static nth_b8 test_allocator_realloc_zero(void) {
    NthFreeList list;
    NthSpan s = chunk(0, 4096, 64);

    NTH_TEST_ASSERT(nth_setup_free_list(&list, s) == NTH_RESULT_OK);

    NthAllocator allocator = nth_free_list_as_allocator(&list);

    void *p = allocator.alloc(allocator.ctx, 64, 1);

    NTH_TEST_ASSERT(p != NULL);

    NTH_TEST_ASSERT(
        allocator.realloc(allocator.ctx, p, 0, 1) == NULL
    );

    NTH_TEST_ASSERT(list.p_used == NULL);

    nth_teardown_free_list(&list);

    return NTH_TRUE;
}


/* ================================================================================ */
/*  STRESS                                                                          */
/* ================================================================================ */

static unsigned g_rs = 88675123u;

static unsigned rnd(void) {
    g_rs = g_rs * 1664525u + 1013904223u;
    return g_rs >> 8;
}


static nth_b8 test_stress_model(void) {
    NthFreeList list;

    NTH_TEST_ASSERT(
        nth_setup_free_list(&list, chunk(0, 4096, 64))
        == NTH_RESULT_OK
    );

    NTH_TEST_ASSERT(
        nth_free_list_grow(&list, chunk(4096, 4096, 64))
    );

    NTH_TEST_ASSERT(
        nth_free_list_grow(&list, chunk(8192, 4096, 64))
    );

    nth_u8   *blk[MAX_BLK];
    nth_usize len[MAX_BLK];
    nth_usize live = 0;

    nth_usize ok = 0;

    for (nth_usize it = 0; it < STRESS_IT; it++) {
        unsigned op = rnd() % 100;

        if (op < 60 && live < MAX_BLK) {
            nth_usize sz = 1 + rnd() % 256;

            nth_u8 *p = nth_free_list_alloc(&list, sz);

            if (p == NULL)
                continue;

            ok++;

            NTH_TEST_ASSERT(in_any_span(&list, p, sz));

            /*
             * Every live allocation must remain disjoint.
             */
            for (nth_usize i = 0; i < live; i++) {
                NTH_TEST_ASSERT(
                    p + sz <= blk[i] ||
                    blk[i] + len[i] <= p
                );
            }

            /*
             * Verify that the requested memory is writable.
             */
            memset(p, 0x5A, sz);

            blk[live] = p;
            len[live] = sz;
            live++;

        } else if (op < 90 && live > 0) {
            nth_usize idx = rnd() % live;

            nth_free_list_free(&list, blk[idx]);

            /*
             * Remove the block from the model.
             */
            live--;

            blk[idx] = blk[live];
            len[idx] = len[live];

        } else {
            /*
             * clear() invalidates every live allocation.
             */
            nth_free_list_clear(&list);

            live = 0;
        }

        /*
         * Verify that all model allocations are still valid and
         * that their contents weren't corrupted.
         */
        for (nth_usize i = 0; i < live; i++) {
            NTH_TEST_ASSERT(
                in_any_span(&list, blk[i], len[i])
            );

            for (nth_usize j = 0; j < len[i]; j++)
                NTH_TEST_ASSERT(blk[i][j] == 0x5A);
        }
    }

    /*
     * The exact amount of successful allocations isn't a property
     * of the allocator, but make sure the randomized test actually
     * exercised it.
     */
    NTH_TEST_ASSERT(ok > 1000);

    nth_teardown_free_list(&list);

    return NTH_TRUE;
}


/* ================================================================================ */
/*  MAIN                                                                            */
/* ================================================================================ */

int main(void) {
    NthTest tests[] = {
        { "alloc/basic",              test_alloc_basic              },
        { "alloc/multiple",           test_alloc_multiple           },
        { "alloc/alignment",          test_alloc_alignment          },
        { "alloc/granularity",        test_alloc_granularity         },

        { "free/reuse",               test_free_reuses_block        },
        { "free/reuse_fragment",      test_free_reuses_fragment     },

        { "coalesce/forward",         test_coalesce_forward         },
        { "coalesce/backward",        test_coalesce_backward        },
        { "coalesce/both",            test_coalesce_both            },

        { "capacity/failure",         test_allocation_failure       },
        { "capacity/exact_fill",      test_exact_fill               },

        { "grow/basic",               test_grow                     },
        { "grow/uses_new_span",       test_grow_uses_new_span       },
        { "grow/past_capacity",       test_grow_past_initial_capacity },

        // { "shrink/returns_last",      test_shrink_returns_last      },
        // { "shrink/refuses_live",      test_shrink_refuses_live      },
        { "shrink/after_free",        test_shrink_after_free        },
        { "shrink/empty",             test_shrink_empty              },

        { "clear/basic",              test_clear                    },

        { "allocator/basic",          test_as_allocator             },
        { "allocator/realloc",        test_allocator_realloc        },
        { "allocator/realloc_null",   test_allocator_realloc_null   },
        { "allocator/realloc_zero",   test_allocator_realloc_zero   },

        { "stress/model",             test_stress_model             },
    };

    return NTH_RUN_TESTS(tests);
}
