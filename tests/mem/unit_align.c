#include "nth_test.h"

#include <narthex/inl/align.h>

#define UPTR_BITS (sizeof(nth_uptr) * 8)
#define MAX_SHIFT 13
#define SWEEP     4096


static nth_b8 test_is_pow2(void) {
    NTH_TEST_ASSERT(!nth_is_pow2(0));

    for (nth_usize i = 0; i < UPTR_BITS; i++)
        NTH_TEST_ASSERT(nth_is_pow2((nth_uptr)1 << i));

    NTH_TEST_ASSERT(!nth_is_pow2(3));
    NTH_TEST_ASSERT(!nth_is_pow2(6));
    NTH_TEST_ASSERT(!nth_is_pow2(96));
    NTH_TEST_ASSERT(!nth_is_pow2(NTH_UPTR_MAX));
    NTH_TEST_ASSERT(!nth_is_pow2(NTH_UPTR_MAX - 1));

    return NTH_TRUE;
}

static nth_b8 test_pad_invariants(void) {
    for (nth_usize s = 0; s < MAX_SHIFT; s++) {
        nth_usize align = (nth_usize)1 << s;

        for (nth_uptr a = 0; a < SWEEP; a++) {
            nth_usize pad = nth_align_pad(a, align);

            NTH_TEST_ASSERT(pad < align);
            NTH_TEST_ASSERT(((a + pad) & (nth_uptr)(align - 1)) == 0);
        }
    }
    return NTH_TRUE;
}

static nth_b8 test_pad_near_uptr_max(void) {
    for (nth_usize s = 0; s < MAX_SHIFT; s++) {
        nth_usize align = (nth_usize)1 << s;

        for (nth_usize k = 0; k < 1024; k++) {
            nth_uptr a   = NTH_UPTR_MAX - k;
            nth_usize pad = nth_align_pad(a, align);

            NTH_TEST_ASSERT(pad < align);
            NTH_TEST_ASSERT(((a + pad) & (nth_uptr)(align - 1)) == 0);
        }
    }
    return NTH_TRUE;
}

static nth_b8 test_pad_matches_align_up(void) {
    for (nth_usize s = 0; s < MAX_SHIFT; s++) {
        nth_usize align = (nth_usize)1 << s;

        for (nth_uptr a = 0; a < SWEEP; a++)
            NTH_TEST_ASSERT(nth_align_pad(a, align) == nth_align_up(a, align) - a);
    }
    return NTH_TRUE;
}

static nth_b8 test_align_up_down(void) {
    for (nth_usize s = 0; s < MAX_SHIFT; s++) {
        nth_usize align = (nth_usize)1 << s;

        for (nth_uptr a = 0; a < SWEEP; a++) {
            nth_uptr up = nth_align_up(a, align);
            nth_uptr dn = nth_align_down(a, align);

            NTH_TEST_ASSERT(nth_is_aligned(up, align));
            NTH_TEST_ASSERT(nth_is_aligned(dn, align));
            NTH_TEST_ASSERT(dn <= a);
            NTH_TEST_ASSERT(a <= up);
            NTH_TEST_ASSERT(up - dn <= align);

            if (nth_is_aligned(a, align)) {
                NTH_TEST_ASSERT(up == a);
                NTH_TEST_ASSERT(dn == a);
            }
        }
    }
    return NTH_TRUE;
}

static nth_b8 test_is_aligned_agrees_with_pad(void) {
    for (nth_usize s = 0; s < MAX_SHIFT; s++) {
        nth_usize align = (nth_usize)1 << s;

        for (nth_uptr a = 0; a < SWEEP; a++) {
            nth_b8 aligned = nth_is_aligned(a, align);
            nth_b8 nopad   = (nth_align_pad(a, align) == 0) ? NTH_TRUE : NTH_FALSE;

            NTH_TEST_ASSERT(aligned == nopad);
        }
    }
    return NTH_TRUE;
}


int main(void) {
    NthTest tests[] = {
        { "is_pow2",                test_is_pow2                },
        { "pad/invariants",         test_pad_invariants         },
        { "pad/near_uptr_max",      test_pad_near_uptr_max      },
        { "pad/matches_align_up",   test_pad_matches_align_up   },
        { "align_up_down",          test_align_up_down          },
        { "is_aligned/agrees_pad",  test_is_aligned_agrees_with_pad },
    };

    return NTH_RUN_TESTS(tests);
}
