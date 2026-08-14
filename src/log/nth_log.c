#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
    #define _POSIX_C_SOURCE 200809L
#endif

#include <narthex/nth_log.h>

#include <narthex/inl/atomic.h>
#include <narthex/narthex.h>
#include <narthex/utils/arch.h>
#include <narthex/utils/compiler.h>
#include <narthex/utils/platform.h>

#include "lifecycle.h"
#include "log_internal.h"
#include "mutex.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if NTH_PLATFORM_WINDOWS
    #include <windows.h>
#else
    #include <unistd.h>
#endif


#define LOG_TIME_SIZE   19
#define LOG_LEVEL_SIZE  5
#define LOG_PREFIX_SIZE (LOG_TIME_SIZE + 1 + LOG_LEVEL_SIZE + 3)
#define LOG_FIXED_SIZE  (LOG_PREFIX_SIZE + 1)

#define LOG_BUFFER_DEFAULT (64u * 1024u)
#define LOG_FORMAT_MAX     2048


/* life and inflight are the fields touched outside the lock, so each gets its
   own cache line to keep it from invalidating the lock holder's reads. */
static NTH_ALIGNAS(NTH_CACHELINE) struct {
    NTH_CACHE_ISOLATE(volatile NthLifecycle, life);
    NTH_CACHE_ISOLATE(volatile nth_u32, inflight);

    char      *buf;
    nth_usize  buf_size;
    nth_usize  buf_index;
    time_t     last_time;
    NthMutex   lock;
    char       time[LOG_TIME_SIZE];
    nth_b8     flush_each;
} g_log;


/* ================================================================================ */
/*  FORMATTERS                                                                      */
/* ================================================================================ */

static void log_update_time(time_t now) {
    struct tm tmv;

#if NTH_PLATFORM_WINDOWS
    if (localtime_s(&tmv, &now) != 0)
        return;
#else
    if (localtime_r(&now, &tmv) == NULL)
        return;
#endif

    const nth_u16 year = (nth_u16)(tmv.tm_year + 1900);
    const nth_u16 day  = (nth_u16)(tmv.tm_yday + 1);
    char *t = g_log.time;

    t[0]  = '[';

    t[1]  = (char)((year / 1000) % 10 + '0');
    t[2]  = (char)((year / 100) % 10 + '0');
    t[3]  = (char)((year / 10) % 10 + '0');
    t[4]  = (char)((year) % 10 + '0');
    t[5]  = '-';
    t[6]  = (char)((day / 100) % 10 + '0');
    t[7]  = (char)((day / 10) % 10 + '0');
    t[8]  = (char)((day) % 10 + '0');

    t[9]  = ' ';

    t[10] = (char)((tmv.tm_hour / 10) % 10 + '0');
    t[11] = (char)((tmv.tm_hour) % 10 + '0');
    t[12] = ':';
    t[13] = (char)((tmv.tm_min / 10) % 10 + '0');
    t[14] = (char)((tmv.tm_min) % 10 + '0');
    t[15] = ':';
    t[16] = (char)((tmv.tm_sec / 10) % 10 + '0');
    t[17] = (char)((tmv.tm_sec) % 10 + '0');

    t[18] = ']';
}

static void log_write_prefix(nth_usize index, NthLogLevel level, const char *stamp) {
    static const char LEVEL_NAME[5][3] = {
        "FTL",
        "ERR",
        "WRN",
        "MSG",
        "DBG"
    };

    char *b = g_log.buf + index;

    memcpy(b, stamp, LOG_TIME_SIZE);
    b += LOG_TIME_SIZE;

    *b++ = ' ';
    *b++ = '[';
    *b++ = LEVEL_NAME[level][0];
    *b++ = LEVEL_NAME[level][1];
    *b++ = LEVEL_NAME[level][2];
    *b++ = ']';
    *b++ = ' ';
    *b++ = '-';
    *b++ = ' ';
}


/* ================================================================================ */
/*  OUTPUT                                                                          */
/* ================================================================================ */

static void log_raw_write(const char *p, nth_usize n) {
#if NTH_PLATFORM_WINDOWS
    HANDLE h = GetStdHandle(STD_ERROR_HANDLE);

    while (n != 0) {
        const DWORD chunk = (DWORD)(n > 0x7FFFFFFFu ? 0x7FFFFFFFu : n);
        DWORD wrote = 0;

        if (!WriteFile(h, p, chunk, &wrote, NULL) || wrote == 0)
            return;

        p += wrote;
        n -= wrote;
    }
#else
    while (n != 0) {
        const ssize_t wrote = write(STDERR_FILENO, p, n);

        if (wrote < 0) {
            if (errno == EINTR)
                continue;
            return;
        }
        if (wrote == 0)
            return;

        p += (nth_usize)wrote;
        n -= (nth_usize)wrote;
    }
#endif
}

/* Caller holds the lock. Waits for in flight writers before touching the buffer. */
static void log_flush_locked(void) {
    while (nth_atomic_load_acquire_u32(&g_log.inflight) != 0)
        ;

    if (g_log.buf_index != 0) {
        log_raw_write(g_log.buf, g_log.buf_index);
        g_log.buf_index = 0;
    }
}


/* ================================================================================ */
/*  LOG IMPLEMENTATION                                                              */
/* ================================================================================ */

static void log_append(NthLogLevel level, const char *msg, nth_usize len) {
    if (NTH_UNLIKELY(!nth_lifecycle_is_alive(&g_log.life)))
        return;
    if (NTH_UNLIKELY(level > NTH_LOG_LEVEL_DEBUG))
        level = NTH_LOG_LEVEL_DEBUG;

    const nth_usize cap = g_log.buf_size;
    nth_usize full = LOG_FIXED_SIZE + len;
    char stamp[LOG_TIME_SIZE];

    if (NTH_UNLIKELY(full > cap)) {
        len  = cap - LOG_FIXED_SIZE;
        full = cap;
    }

    const time_t now = time(NULL);

    nth_mutex_lock(&g_log.lock);

    if (NTH_UNLIKELY(cap - g_log.buf_index < full))
        log_flush_locked();

    if (NTH_UNLIKELY(now > g_log.last_time)) {
        g_log.last_time = now;
        log_update_time(now);
    }
    memcpy(stamp, g_log.time, LOG_TIME_SIZE);

    const nth_usize i = g_log.buf_index;
    g_log.buf_index = i + full;
    nth_atomic_fetch_add_relaxed_u32(&g_log.inflight, 1);

    nth_mutex_unlock(&g_log.lock);

    log_write_prefix(i, level, stamp);
    memcpy(g_log.buf + i + LOG_PREFIX_SIZE, msg, len);
    g_log.buf[i + full - 1] = '\n';

    nth_atomic_fetch_sub_u32(&g_log.inflight, 1);

    if (g_log.flush_each)
        nth_flush();
}

void nth_log(NthLogLevel level, const char *msg) {
    if (NTH_UNLIKELY(msg == NULL))
        return;

    log_append(level, msg, strlen(msg));
}

void nth_logn(NthLogLevel level, const char *msg, nth_usize len) {
    if (NTH_UNLIKELY(msg == NULL))
        return;

    log_append(level, msg, len);
}

void nth_logf(NthLogLevel level, const char *fmt, ...) {
    char tmp[LOG_FORMAT_MAX];
    va_list ap;
    int n;

    if (NTH_UNLIKELY(fmt == NULL))
        return;

    va_start(ap, fmt);
    n = vsnprintf(tmp, sizeof tmp, fmt, ap);
    va_end(ap);

    if (NTH_UNLIKELY(n < 0))
        return;

    nth_usize len = (nth_usize)n;
    if (len >= sizeof tmp)
        len = sizeof tmp - 1;

    log_append(level, tmp, len);
}

void nth_flush(void) {
    if (NTH_UNLIKELY(!nth_lifecycle_is_alive(&g_log.life)))
        return;

    nth_mutex_lock(&g_log.lock);
    log_flush_locked();
    nth_mutex_unlock(&g_log.lock);
}


/* ================================================================================ */
/*  LIFE-CYCLE                                                                      */
/* ================================================================================ */

NthResult nth_init_log(const NthLoggerDesc *desc) {
    NthResult r = nth_lifecycle_begin_init(&g_log.life);
    if (r != NTH_RESULT_OK)
        return r;

    nth_usize size = LOG_BUFFER_DEFAULT;
    nth_b8 flush_each = NTH_FALSE;

    if (desc != NULL) {
        if (desc->buffer_size != 0)
            size = desc->buffer_size;
        flush_each = desc->flush_each;
    }
    if (size <= LOG_FIXED_SIZE) {
        nth_lifecycle_fail_init(&g_log.life);
        return NTH_RESULT_INVALID_ARGUMENT;
    }

    g_log.buf = (char *)malloc(size);
    if (g_log.buf == NULL) {
        nth_lifecycle_fail_init(&g_log.life);
        return NTH_RESULT_OUT_OF_MEMORY;
    }

    g_log.buf_size   = size;
    g_log.buf_index  = 0;
    g_log.last_time  = (time_t)-1;
    g_log.inflight   = 0;
    g_log.flush_each = flush_each;

    nth_mutex_init(&g_log.lock);
    nth_lifecycle_end_init(&g_log.life);

    return NTH_RESULT_OK;
}

void nth_term_log(void) {
    if (nth_lifecycle_begin_term(&g_log.life) != NTH_RESULT_OK)
        return;

    nth_mutex_lock(&g_log.lock);
    log_flush_locked();
    nth_mutex_unlock(&g_log.lock);

    nth_mutex_destroy(&g_log.lock);

    free(g_log.buf);
    g_log.buf = NULL;
    g_log.buf_size = 0;

    nth_lifecycle_end_term(&g_log.life);
}

nth_b8 nth_log_is_alive(void) {
    return nth_lifecycle_is_alive(&g_log.life);
}
