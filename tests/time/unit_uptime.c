#include <narthex/nth_uptime.h>
#include "nth_test.h"

#include <math.h>


static nth_b8 test_delta_since_normal(void) {
    NthUptimeNs prev = NTH_UPTIME_NS(100);
    NthUptimeNs now  = NTH_UPTIME_NS(250);

    NthDeltaNs dt = NTH_DELTA_SINCE(prev, now);

    NTH_TEST_ASSERT(dt.ns == 150);
    return NTH_TRUE;
}

static nth_b8 test_delta_since_saturates_on_clock_going_backward(void) {
    NthUptimeNs prev = NTH_UPTIME_NS(250);
    NthUptimeNs now  = NTH_UPTIME_NS(100); /* clock somehow went backward */

    NthDeltaNs dt = NTH_DELTA_SINCE(prev, now);

    NTH_TEST_ASSERT(dt.ns == 0);
    return NTH_TRUE;
}

static nth_b8 test_delta_since_zero_when_equal(void) {
    NthUptimeNs t = NTH_UPTIME_NS(42);

    NthDeltaNs dt = NTH_DELTA_SINCE(t, t);

    NTH_TEST_ASSERT(dt.ns == 0);
    return NTH_TRUE;
}

static nth_b8 test_delta_clamp_clamps_when_over(void) {
    NthDeltaNs dt = NTH_DELTA_NS(500);

    NthDeltaNs clamped = NTH_DELTA_CLAMP(dt, 200);

    NTH_TEST_ASSERT(clamped.ns == 200);
    return NTH_TRUE;
}

static nth_b8 test_delta_clamp_passthrough_when_under(void) {
    NthDeltaNs dt = NTH_DELTA_NS(100);

    NthDeltaNs clamped = NTH_DELTA_CLAMP(dt, 200);

    NTH_TEST_ASSERT(clamped.ns == 100);
    return NTH_TRUE;
}

static nth_b8 test_delta_clamp_exact_boundary(void) {
    NthDeltaNs dt = NTH_DELTA_NS(200);

    NthDeltaNs clamped = NTH_DELTA_CLAMP(dt, 200);

    NTH_TEST_ASSERT(clamped.ns == 200);
    return NTH_TRUE;
}

static nth_b8 test_delta_sub_saturating_normal(void) {
    nth_u64 remaining = 300;
    NthDeltaNs dt = NTH_DELTA_NS(100);

    remaining = NTH_DELTA_SUB_SATURATING(remaining, dt);

    NTH_TEST_ASSERT(remaining == 200);
    return NTH_TRUE;
}

static nth_b8 test_delta_sub_saturating_floors_at_zero(void) {
    nth_u64 remaining = 50;
    NthDeltaNs dt = NTH_DELTA_NS(100);

    remaining = NTH_DELTA_SUB_SATURATING(remaining, dt);

    NTH_TEST_ASSERT(remaining == 0);
    return NTH_TRUE;
}

static nth_b8 test_delta_to_sec_f64(void) {
    NthDeltaNs dt = NTH_DELTA_NS(1500000000); /* 1.5s */

    nth_f64 sec = NTH_DELTA_TO_SEC_F64(dt);

    NTH_TEST_ASSERT(fabs(sec - 1.5) < 1e-9);
    return NTH_TRUE;
}

static nth_b8 test_delta_to_sec_f32(void) {
    NthDeltaNs dt = NTH_DELTA_NS(250000000); /* 0.25s */

    nth_f32 sec = NTH_DELTA_TO_SEC_F32(dt);

    NTH_TEST_ASSERT(fabsf(sec - 0.25f) < 1e-6f);
    return NTH_TRUE;
}

static nth_b8 test_delta_to_sec_zero(void) {
    NthDeltaNs dt = NTH_DELTA_NS(0);

    NTH_TEST_ASSERT(NTH_DELTA_TO_SEC_F64(dt) == 0.0);
    return NTH_TRUE;
}

static nth_b8 test_uptime_now_is_monotonic(void) {
    NthUptimeNs a = nth_uptime_now();
    NthUptimeNs b = nth_uptime_now();

    NTH_TEST_ASSERT(b.ns >= a.ns);
    return NTH_TRUE;
}

static nth_b8 test_uptime_elapsed_matches_manual_delta(void) {
    NthUptimeNs start = nth_uptime_now();
    NthUptimeNs end   = nth_uptime_now();

    NthDeltaNs elapsed = nth_uptime_elapsed(start);
    NthDeltaNs manual  = NTH_DELTA_SINCE(start, end);

    /* elapsed was measured slightly after `end`, so it should be >= the
     * manual delta, and not off by some absurd amount (1 second is a very
     * generous ceiling just to catch a genuinely broken clock source). */
    NTH_TEST_ASSERT(elapsed.ns >= manual.ns);
    NTH_TEST_ASSERT(elapsed.ns - manual.ns < 1000000000ULL);
    return NTH_TRUE;
}

static nth_b8 test_uptime_elapsed_never_negative_looking(void) {
    NthUptimeNs start = nth_uptime_now();
    NthDeltaNs elapsed = nth_uptime_elapsed(start);

    /* nth_u64 can't actually go negative, but this guards against the
     * saturating-subtraction logic being removed/broken later and elapsed
     * wrapping around to a huge value instead. */
    NTH_TEST_ASSERT(elapsed.ns < 1000000000ULL); /* took less than 1s to run this line */
    return NTH_TRUE;
}


int main(void) {
    NthTest tests[] = {
        { "delta/since_normal",              test_delta_since_normal                       },
        { "delta/since_saturates_backward",  test_delta_since_saturates_on_clock_going_backward },
        { "delta/since_zero_when_equal",     test_delta_since_zero_when_equal              },
        { "delta/clamp_clamps_when_over",    test_delta_clamp_clamps_when_over             },
        { "delta/clamp_passthrough_when_under", test_delta_clamp_passthrough_when_under    },
        { "delta/clamp_exact_boundary",      test_delta_clamp_exact_boundary               },
        { "delta/sub_saturating_normal",     test_delta_sub_saturating_normal              },
        { "delta/sub_saturating_floors",     test_delta_sub_saturating_floors_at_zero      },
        { "delta/to_sec_f64",                test_delta_to_sec_f64                         },
        { "delta/to_sec_f32",                test_delta_to_sec_f32                         },
        { "delta/to_sec_zero",                test_delta_to_sec_zero                       },
        { "uptime/now_is_monotonic",         test_uptime_now_is_monotonic                  },
        { "uptime/elapsed_matches_manual",   test_uptime_elapsed_matches_manual_delta      },
        { "uptime/elapsed_never_negative_looking", test_uptime_elapsed_never_negative_looking },
    };

    return NTH_RUN_TESTS(tests);
}