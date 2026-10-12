#ifndef NTH_TIME_H
#define NTH_TIME_H

#include <narthex/nth_types.h>
#include <narthex/nth_uptime.h>
#include <narthex/utils/api.h>
#include <narthex/utils/compiler.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef struct NthTime { nth_i64 ns; } NthTime; /* nanoseconds since Unix epoch, UTC */

typedef nth_u8 NthWeekday;
enum {
    NTH_WEEKDAY_SUNDAY = 0, NTH_WEEKDAY_MONDAY, NTH_WEEKDAY_TUESDAY, NTH_WEEKDAY_WEDNESDAY,
    NTH_WEEKDAY_THURSDAY, NTH_WEEKDAY_FRIDAY, NTH_WEEKDAY_SATURDAY,
};

typedef struct NthCalendar {
    nth_i32    year;       /* years since 0BC/AC */
    nth_u8     month;      /* 1-12 */
    nth_u8     day;        /* 1-31 */
    nth_u8     hour;       /* 0-23 */
    nth_u8     minute;     /* 0-59 */
    nth_u8     second;     /* 0-59 */
    nth_u32    nanosecond; /* 0-999999999 */
    NthWeekday weekday;
    nth_u16    yearday;
    nth_i32    utc_offset_seconds;
} NthCalendar;


NTH_API NthTime nth_time_now(void);

NTH_API void nth_time_to_calendar(NthTime time, NthCalendar *out);
NTH_API void nth_time_to_calendar_local(NthTime time, NthCalendar *out);

NTH_API NthTime nth_time_from_calendar(const NthCalendar *cal);
NTH_API NthTime nth_time_from_calendar_local(const NthCalendar *cal);


static inline nth_b8 nth_time_before(NthTime a, NthTime b) { return a.ns < b.ns; }
static inline nth_b8 nth_time_after(NthTime a, NthTime b)  { return a.ns > b.ns; }
static inline nth_b8 nth_time_equal(NthTime a, NthTime b)  { return a.ns == b.ns; }

static inline NthDeltaNs nth_time_diff(NthTime a, NthTime b) {
    return (NthDeltaNs){ .ns = (a.ns > b.ns) ? (nth_u64)(a.ns - b.ns) : 0 };
}
static inline NthTime nth_time_add(NthTime t, NthDeltaNs delta) {
    return (NthTime){ .ns = t.ns + (nth_i64)delta.ns };
}
static inline NthTime nth_time_sub(NthTime t, NthDeltaNs delta) {
    return (NthTime){ .ns = t.ns - (nth_i64)delta.ns };
}


#ifdef __cplusplus
}
#endif

#endif /* NTH_TIME_H */