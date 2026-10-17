#include "nth_test.h"

#include <narthex/narthex.h>
#include <narthex/nth_time.h>


static nth_b8 test_epoch_is_thursday(void) {
    NthCalendar cal;
    nth_time_to_calendar((NthTime){ .ns = 0 }, &cal);

    NTH_TEST_ASSERT(cal.year == 1970);
    NTH_TEST_ASSERT(cal.month == 1);
    NTH_TEST_ASSERT(cal.day == 1);
    NTH_TEST_ASSERT(cal.weekday == NTH_WEEKDAY_THURSDAY);
    NTH_TEST_ASSERT(cal.yearday == 0);
    return NTH_TRUE;
}

static nth_b8 test_round_trip_modern_date(void) {
    NthCalendar in = {0};
    in.year = 2024; in.month = 1; in.day = 1;
    in.hour = 13; in.minute = 45; in.second = 30; in.nanosecond = 123456789;

    NthTime t = nth_time_from_calendar(&in);

    NthCalendar out;
    nth_time_to_calendar(t, &out);

    NTH_TEST_ASSERT(out.year == 2024 && out.month == 1 && out.day == 1);
    NTH_TEST_ASSERT(out.hour == 13 && out.minute == 45 && out.second == 30);
    NTH_TEST_ASSERT(out.nanosecond == 123456789);
    NTH_TEST_ASSERT(out.weekday == NTH_WEEKDAY_MONDAY); /* 2024-01-01 was a Monday */
    return NTH_TRUE;
}

static nth_b8 test_leap_year_yearday(void) {
    NthCalendar in = {0};
    in.year = 2024; in.month = 2; in.day = 29; /* 2024 is a leap year */

    NthTime t = nth_time_from_calendar(&in);
    NthCalendar out;
    nth_time_to_calendar(t, &out);

    NTH_TEST_ASSERT(out.month == 2 && out.day == 29);
    NTH_TEST_ASSERT(out.yearday == 59); /* 31 (Jan) + 28 days into Feb = index 59 */
    return NTH_TRUE;
}

static nth_b8 test_non_leap_year_rejects_feb29_drift(void) {
    NthCalendar in = {0};
    in.year = 2023; in.month = 3; in.day = 1; /* 2023 is not a leap year */

    NthTime t = nth_time_from_calendar(&in);
    NthCalendar out;
    nth_time_to_calendar(t, &out);

    NTH_TEST_ASSERT(out.yearday == 59); /* 31 (Jan) + 28 (Feb, non-leap) = index 59 for Mar 1 */
    return NTH_TRUE;
}

static nth_b8 test_pre_epoch_date(void) {
    /* One second before the epoch — exercises the floor-division branch in
       nth_time_to_calendar, the trickiest part of that function. */
    NthTime t = { .ns = -1000000000LL };

    NthCalendar cal;
    nth_time_to_calendar(t, &cal);

    NTH_TEST_ASSERT(cal.year == 1969 && cal.month == 12 && cal.day == 31);
    NTH_TEST_ASSERT(cal.hour == 23 && cal.minute == 59 && cal.second == 59);
    NTH_TEST_ASSERT(cal.weekday == NTH_WEEKDAY_WEDNESDAY);
    return NTH_TRUE;
}

static nth_b8 test_pre_epoch_round_trip(void) {
    NthCalendar in = {0};
    in.year = 1969; in.month = 12; in.day = 31;
    in.hour = 23; in.minute = 59; in.second = 59; in.nanosecond = 500000000;

    NthTime t = nth_time_from_calendar(&in);
    NTH_TEST_ASSERT(t.ns == -500000000LL);

    NthCalendar out;
    nth_time_to_calendar(t, &out);
    NTH_TEST_ASSERT(out.year == 1969 && out.month == 12 && out.day == 31);
    NTH_TEST_ASSERT(out.hour == 23 && out.minute == 59 && out.second == 59);
    NTH_TEST_ASSERT(out.nanosecond == 500000000);
    return NTH_TRUE;
}

static nth_b8 test_ordering_and_equality(void) {
    NthTime a = { .ns = 1000 };
    NthTime b = { .ns = 2000 };

    NTH_TEST_ASSERT(nth_time_before(a, b));
    NTH_TEST_ASSERT(nth_time_after(b, a));
    NTH_TEST_ASSERT(!nth_time_equal(a, b));
    NTH_TEST_ASSERT(nth_time_equal(a, a));
    return NTH_TRUE;
}

static nth_b8 test_diff_add_sub_round_trip(void) {
    NthTime a = { .ns = 5000000000LL };
    NthTime b = { .ns = 8000000000LL };

    NthDeltaNs d = nth_time_diff(b, a);
    NTH_TEST_ASSERT(d.ns == 3000000000ULL);

    NthTime b2 = nth_time_add(a, d);
    NTH_TEST_ASSERT(nth_time_equal(b2, b));

    NthTime a2 = nth_time_sub(b, d);
    NTH_TEST_ASSERT(nth_time_equal(a2, a));
    return NTH_TRUE;
}

static nth_b8 test_diff_saturates_at_zero(void) {
    NthTime earlier = { .ns = 100 };
    NthTime later   = { .ns = 500 };

    NthDeltaNs d = nth_time_diff(earlier, later); /* earlier - later would be negative */
    NTH_TEST_ASSERT(d.ns == 0);
    return NTH_TRUE;
}

static nth_b8 test_now_is_recent(void) {
    NthTime t = nth_time_now();
    NthCalendar cal;
    nth_time_to_calendar(t, &cal);

    /* loose sanity bound, not a precise check: catches a badly wrong epoch/unit bug
       without being fragile about exactly when this test runs */
    NTH_TEST_ASSERT(cal.year >= 2024 && cal.year < 2100);
    return NTH_TRUE;
}

static nth_b8 test_local_round_trip(void) {
    /* Can't assert an exact offset without knowing the test machine's timezone, but
       the round trip itself must hold in any timezone, DST or not. */
    NthTime original = nth_time_now();

    NthCalendar cal;
    nth_time_to_calendar_local(original, &cal);

    NTH_TEST_ASSERT(cal.utc_offset_seconds % 900 == 0); /* every real UTC offset is 15-minute aligned */

    NthTime back = nth_time_from_calendar_local(&cal);
    NTH_TEST_ASSERT(nth_time_equal(back, original));
    return NTH_TRUE;
}

int main(void) {
    NthResult init_r = nth_init(NULL);
    if(init_r != NTH_RESULT_OK) {
        fprintf(stderr, "nth_init failed: %d\n", init_r);
        return EXIT_FAILURE;
    }

    NthTest tests[] = {
        { "calendar/epoch_is_thursday",       test_epoch_is_thursday },
        { "calendar/round_trip_modern",       test_round_trip_modern_date },
        { "calendar/leap_year_yearday",       test_leap_year_yearday },
        { "calendar/non_leap_year_yearday",   test_non_leap_year_rejects_feb29_drift },
        { "calendar/pre_epoch_date",          test_pre_epoch_date },
        { "calendar/pre_epoch_round_trip",    test_pre_epoch_round_trip },
        { "arithmetic/ordering_equality",     test_ordering_and_equality },
        { "arithmetic/diff_add_sub",          test_diff_add_sub_round_trip },
        { "arithmetic/diff_saturates",        test_diff_saturates_at_zero },
        { "now/is_recent",                    test_now_is_recent },
        { "local/round_trip",                 test_local_round_trip },
    };

    const int rc = NTH_RUN_TESTS(tests);

    nth_term();
    return rc;
}