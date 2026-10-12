#include <narthex/nth_time.h>

#include <narthex/inl/lifecycle.h>
#include <narthex/utils/check.h>
#include <narthex/utils/platform.h>

#include "internal.h"

#if NTH_PLATFORM_WINDOWS
#include <windows.h>
#else
#include <time.h>
#endif


#define NS_PER_SEC  ((nth_i64)1000000000)
#define SEC_PER_DAY ((nth_i64)86400)
#define NS_PER_DAY  (SEC_PER_DAY * NS_PER_SEC)


/* ================================================================================ */
/*  CIVIL CALENDAR MATH                                                             */
/*  http://howardhinnant.github.io/date_algorithms.html — public domain.            */
/* ================================================================================ */

static nth_i64 days_from_civil(nth_i64 y, nth_u32 m, nth_u32 d) {
    y -= (m <= 2);
    nth_i64 era = (y >= 0 ? y : y - 399) / 400;
    nth_u32 yoe = (nth_u32)(y - era * 400);                         /* [0, 399] */
    nth_u32 doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;   /* [0, 365] */
    nth_u32 doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;            /* [0, 146096] */
    return era * 146097 + (nth_i64)doe - 719468;                    /* 719468 = days from 0000-03-01 to 1970-01-01 */
}

static void civil_from_days(nth_i64 z, nth_i64 *out_y, nth_u32 *out_m, nth_u32 *out_d) {
    z += 719468;
    nth_i64 era = (z >= 0 ? z : z - 146096) / 146097;
    nth_u32 doe = (nth_u32)(z - era * 146097);                              /* [0, 146096] */
    nth_u32 yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;    /* [0, 399] */
    nth_i64 y   = (nth_i64)yoe + era * 400;
    nth_u32 doy = doe - (365 * yoe + yoe / 4 - yoe / 100);                  /* [0, 365] */
    nth_u32 mp  = (5 * doy + 2) / 153;                                      /* [0, 11], march-based */
    nth_u32 d   = doy - (153 * mp + 2) / 5 + 1;                             /* [1, 31] */
    nth_u32 m   = mp + (mp < 10 ? 3 : -9);                                  /* [1, 12], march-based -> real */

    *out_y = y + (m <= 2);
    *out_m = m;
    *out_d = d;
}

static NthWeekday weekday_from_days(nth_i64 z) {
    /* 1970-01-01 (z == 0) was a Thursday. */
    nth_i64 w = (z >= -4) ? (z + 4) % 7 : (z + 5) % 7 + 6;
    return (NthWeekday)w;
}


/* ================================================================================ */
/*  IMPLEMENTATION                                                                  */
/* ================================================================================ */

NthTime nth_time_now(void) {
    NTH_ASSERT(NTH_LIKELY(nth_lifecycle_is_alive(&g_narthex_life)));

#if NTH_PLATFORM_WINDOWS
    FILETIME ft;
    GetSystemTimePreciseAsFileTime(&ft);
    nth_u64 ticks = ((nth_u64)ft.dwHighDateTime << 32) | ft.dwLowDateTime; /* 100ns ticks since 1601-01-01 */
    const nth_i64 EPOCH_DIFF_100NS = 116444736000000000LL; /* 1601-01-01 -> 1970-01-01 */
    return (NthTime){ .ns = ((nth_i64)ticks - EPOCH_DIFF_100NS) * 100 };
#else
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (NthTime){ .ns = (nth_i64)ts.tv_sec * NS_PER_SEC + ts.tv_nsec };
#endif
}

void nth_time_to_calendar(NthTime time, NthCalendar *out) {
    NTH_DASSERT(NTH_LIKELY(out != NULL));
    NTH_ASSERT(NTH_LIKELY(nth_lifecycle_is_alive(&g_narthex_life)));

    nth_i64 days      = time.ns / NS_PER_DAY;
    nth_i64 ns_of_day = time.ns % NS_PER_DAY;
    if(ns_of_day < 0) {        /* floor division, not truncation, for pre-1970 instants */
        ns_of_day += NS_PER_DAY;
        days -= 1;
    }

    nth_i64 y;
    nth_u32 m, d;
    civil_from_days(days, &y, &m, &d);

    nth_i64 sec_of_day = ns_of_day / NS_PER_SEC;

    out->year       = (nth_i32)y;
    out->month      = (nth_u8)m;
    out->day        = (nth_u8)d;
    out->hour       = (nth_u8)(sec_of_day / 3600);
    out->minute     = (nth_u8)((sec_of_day / 60) % 60);
    out->second     = (nth_u8)(sec_of_day % 60);
    out->nanosecond = (nth_u32)(ns_of_day % NS_PER_SEC);
    out->weekday    = weekday_from_days(days);
    out->yearday    = (nth_u16)(days - days_from_civil(y, 1, 1));
}
void nth_time_to_calendar_local(NthTime time, NthCalendar *out) {
    NTH_DASSERT(NTH_LIKELY(out != NULL));
    NTH_ASSERT(NTH_LIKELY(nth_lifecycle_is_alive(&g_narthex_life)));

#if NTH_PLATFORM_WINDOWS
    const nth_i64 EPOCH_DIFF_100NS = 116444736000000000LL;
    nth_i64 ticks = time.ns / 100 + EPOCH_DIFF_100NS;

    FILETIME utc_ft;
    utc_ft.dwLowDateTime  = (DWORD)(ticks & 0xFFFFFFFF);
    utc_ft.dwHighDateTime = (DWORD)(ticks >> 32);

    SYSTEMTIME utc_st, local_st;
    FileTimeToSystemTime(&utc_ft, &utc_st);
    SystemTimeToTzSpecificLocalTimeEx(NULL, &utc_st, &local_st); /* NULL = current TZ, DST-correct per-date */

    out->year   = local_st.wYear;
    out->month  = (nth_u8)local_st.wMonth;
    out->day    = (nth_u8)local_st.wDay;
    out->hour   = (nth_u8)local_st.wHour;
    out->minute = (nth_u8)local_st.wMinute;
    out->second = (nth_u8)local_st.wSecond;
    /* SYSTEMTIME is millisecond-resolution; recover the sub-ms remainder from the original instant. */
    out->nanosecond = (nth_u32)(local_st.wMilliseconds * 1000000) + (nth_u32)(((time.ns % NS_PER_SEC) + NS_PER_SEC) % 1000000);
    out->weekday = (NthWeekday)local_st.wDayOfWeek; /* 0 = Sunday, matches NthWeekday */
    out->yearday = (nth_u16)(days_from_civil(out->year, out->month, out->day) - days_from_civil(out->year, 1, 1));

    FILETIME local_as_utc_ft;
    SystemTimeToFileTime(&local_st, &local_as_utc_ft);
    nth_i64 local_ticks = ((nth_i64)local_as_utc_ft.dwHighDateTime << 32) | local_as_utc_ft.dwLowDateTime;
    out->utc_offset_seconds = (nth_i32)((local_ticks - ticks) / 10000000);
#else
    time_t secs = (time_t)(time.ns / NS_PER_SEC);
    nth_i64 ns_of_sec = time.ns % NS_PER_SEC;
    if(ns_of_sec < 0) { ns_of_sec += NS_PER_SEC; secs -= 1; }

    struct tm tmv;
    localtime_r(&secs, &tmv);

    out->year       = tmv.tm_year + 1900;
    out->month      = (nth_u8)(tmv.tm_mon + 1);
    out->day        = (nth_u8)tmv.tm_mday;
    out->hour       = (nth_u8)tmv.tm_hour;
    out->minute     = (nth_u8)tmv.tm_min;
    out->second     = (nth_u8)tmv.tm_sec;
    out->nanosecond = (nth_u32)ns_of_sec;
    out->weekday    = (NthWeekday)tmv.tm_wday; /* 0 = Sunday, matches NthWeekday */
    out->yearday    = (nth_u16)tmv.tm_yday;
    out->utc_offset_seconds = (nth_i32)tmv.tm_gmtoff; /* glibc/BSD extension; see note below */
#endif
}

NthTime nth_time_from_calendar(const NthCalendar *cal) {
    NTH_DASSERT(NTH_LIKELY(cal != NULL));
    NTH_ASSERT(NTH_LIKELY(nth_lifecycle_is_alive(&g_narthex_life)));
    NTH_ASSERT(NTH_LIKELY(cal->month >= 1 && cal->month <= 12));
    NTH_ASSERT(NTH_LIKELY(cal->day >= 1 && cal->day <= 31));

    nth_i64 days       = days_from_civil(cal->year, cal->month, cal->day);
    nth_i64 sec_of_day = (nth_i64)cal->hour * 3600 + (nth_i64)cal->minute * 60 + (nth_i64)cal->second;

    return (NthTime){ .ns = days * NS_PER_DAY + sec_of_day * NS_PER_SEC + (nth_i64)cal->nanosecond };
}
NthTime nth_time_from_calendar_local(const NthCalendar *cal) {
    NTH_DASSERT(NTH_LIKELY(cal != NULL));
    NTH_ASSERT(NTH_LIKELY(nth_lifecycle_is_alive(&g_narthex_life)));

#if NTH_PLATFORM_WINDOWS
    SYSTEMTIME local_st = {0};
    local_st.wYear   = (WORD)cal->year;
    local_st.wMonth  = cal->month;
    local_st.wDay    = cal->day;
    local_st.wHour   = cal->hour;
    local_st.wMinute = cal->minute;
    local_st.wSecond = cal->second;
    local_st.wMilliseconds = (WORD)(cal->nanosecond / 1000000);

    SYSTEMTIME utc_st;
    TzSpecificLocalTimeToSystemTimeEx(NULL, &local_st, &utc_st); /* Windows 7+; resolves DST per-date */

    FILETIME utc_ft;
    SystemTimeToFileTime(&utc_st, &utc_ft);
    const nth_i64 EPOCH_DIFF_100NS = 116444736000000000LL;
    nth_i64 ticks = (((nth_i64)utc_ft.dwHighDateTime << 32) | utc_ft.dwLowDateTime) - EPOCH_DIFF_100NS;

    return (NthTime){ .ns = ticks * 100 + (cal->nanosecond % 1000000) };
#else
    struct tm tmv = {0};
    tmv.tm_year = cal->year - 1900;
    tmv.tm_mon  = cal->month - 1;
    tmv.tm_mday = cal->day;
    tmv.tm_hour = cal->hour;
    tmv.tm_min  = cal->minute;
    tmv.tm_sec  = cal->second;
    tmv.tm_isdst = -1; /* let the system resolve DST for this date rather than guessing ourselves */

    time_t secs = mktime(&tmv); /* consults TZ/tzdata; handles historical DST rules correctly */

    return (NthTime){ .ns = (nth_i64)secs * NS_PER_SEC + cal->nanosecond };
#endif
}

// NthTime nth_time_from_calendar_offset(const NthCalendar *cal, nth_i32 utc_offset_seconds) {
//     NTH_DASSERT(NTH_LIKELY(cal != NULL));
// 
//     NthTime as_if_utc = nth_time_from_calendar(cal);
//     return (NthTime){ .ns = as_if_utc.ns - (nth_i64)utc_offset_seconds * NS_PER_SEC };
// }
