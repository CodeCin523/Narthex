#include <narthex/nth_uptime.h>
#include <narthex/inl/lifecycle.h>
#include <narthex/utils/platform.h>
#include <narthex/utils/check.h>

#include "internal.h"

#if NTH_PLATFORM_WINDOWS
#include <windows.h>
#else
#include <time.h>
#endif


static struct {
#if NTH_PLATFORM_WINDOWS
    LARGE_INTEGER start_time;
    LARGE_INTEGER frequency;
#else
    struct timespec start_time;
#endif
} g_uptime;


NthUptimeNs nth_uptime_now(void) {
    NTH_ASSERT(NTH_LIKELY(nth_lifecycle_is_alive(&g_narthex_life)));

    NthUptimeNs up_time={NTH_U64_MAX};

#if NTH_PLATFORM_WINDOWS
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    up_time.ns = ((nth_u64)(now.QuadPart - g_uptime.start_time.QuadPart) * 1000000000ULL) / (nth_u64)g_uptime.frequency.QuadPart;
#else
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    nth_u64 now_ns = (nth_u64)now.tv_sec * 1000000000ULL + (nth_u64)now.tv_nsec;
    nth_u64 start_ns = (nth_u64)g_uptime.start_time.tv_sec * 1000000000ULL + (nth_u64)g_uptime.start_time.tv_nsec;
    up_time.ns = now_ns - start_ns;
#endif

    return up_time;
}
NthDeltaNs nth_uptime_elapsed(NthUptimeNs since) {
    NTH_ASSERT(NTH_LIKELY(nth_lifecycle_is_alive(&g_narthex_life)));

    NthUptimeNs up_time={NTH_U64_MAX};
    NthDeltaNs dt = {NTH_U64_MAX};
    
#if NTH_PLATFORM_WINDOWS
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    up_time.ns = ((nth_u64)(now.QuadPart - g_uptime.start_time.QuadPart) * 1000000000ULL) / (nth_u64)g_uptime.frequency.QuadPart;
#else
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    nth_u64 now_ns = (nth_u64)now.tv_sec * 1000000000ULL + (nth_u64)now.tv_nsec;
    nth_u64 start_ns = (nth_u64)g_uptime.start_time.tv_sec * 1000000000ULL + (nth_u64)g_uptime.start_time.tv_nsec;
    up_time.ns = now_ns - start_ns;
#endif

    dt = NTH_DELTA_SINCE(since, up_time);
    return dt;
}


/* ================================================================================ */
/*  LIFE-CYCLE                                                                      */
/* ================================================================================ */

NthResult nth_init_uptime(void) {
#if NTH_PLATFORM_WINDOWS
    QueryPerformanceFrequency(&g_uptime.frequency);
    QueryPerformanceCounter(&g_uptime.start_time);
#else
    clock_gettime(CLOCK_MONOTONIC, &g_uptime.start_time);
#endif

    return NTH_RESULT_OK;
}
void nth_term_uptime() {
#if NTH_PLATFORM_WINDOWS
    g_uptime.frequency = 0;
    g_uptime.start_time = 0;
#else
    g_uptime.start_time = (struct timespec){0};
#endif
}