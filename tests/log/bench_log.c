#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
    #define _POSIX_C_SOURCE 200809L
#endif

#include <narthex/nth_log.h>

#include <narthex/narthex.h>
#include <narthex/utils/platform.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if NTH_PLATFORM_WINDOWS
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
    #define NULL_DEVICE "NUL"
#else
    #include <pthread.h>
    #include <unistd.h>
    #define NULL_DEVICE "/dev/null"
#endif

/* Declared here until the core exposes them. */
NthResult nth_init_log(const NthLoggerDesc *desc);
void nth_term_log(void);


#define OPS        200000u
#define THREADS    8u
#define OPS_PER_TH (OPS / THREADS)
#define BUF_SIZE   (64u * 1024u)
#define LINE_MAX   256

static const char MSG[] = "the quick brown fox jumps over the lazy dog";


/* ================================================================================ */
/*  HARNESS                                                                         */
/* ================================================================================ */

typedef struct {
    const char *group;
    const char *name;
    double      ms;
    nth_usize   ops;
} BenchResult;

static BenchResult g_res[24];
static nth_usize   g_res_count;

static void record(const char *group, const char *name, double ms, nth_usize ops) {
    g_res[g_res_count].group = group;
    g_res[g_res_count].name  = name;
    g_res[g_res_count].ms    = ms;
    g_res[g_res_count].ops   = ops;
    g_res_count++;
}

static double now_ms(void) {
#if NTH_PLATFORM_WINDOWS
    LARGE_INTEGER f, c;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart * 1000.0 / (double)f.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0;
#endif
}

static void raw_write(const char *p, nth_usize n) {
#if NTH_PLATFORM_WINDOWS
    DWORD wrote = 0;
    WriteFile(GetStdHandle(STD_ERROR_HANDLE), p, (DWORD)n, &wrote, NULL);
#else
    ssize_t r = write(STDERR_FILENO, p, n);
    (void)r;
#endif
}

static void break_time(time_t now, struct tm *out) {
#if NTH_PLATFORM_WINDOWS
    localtime_s(out, &now);
#else
    localtime_r(&now, out);
#endif
}

/* Per caller timestamp cache, so the threaded runs share no state. */
typedef struct {
    time_t at;
    char   text[20];
} Stamp;

#define STAMP_INIT { (time_t)-1, { 0 } }

static void stamp_get(Stamp *s, char *out) {
    const time_t now = time(NULL);

    if (now != s->at) {
        struct tm tmv;
        break_time(now, &tmv);
        strftime(s->text, sizeof s->text, "[%Y-%j %H:%M:%S]", &tmv);
        s->at = now;
    }
    memcpy(out, s->text, 19);
    out[19] = '\0';
}

static void stamp_get_uncached(char *out) {
    struct tm tmv;
    break_time(time(NULL), &tmv);
    strftime(out, 20, "[%Y-%j %H:%M:%S]", &tmv);
}


/* ================================================================================ */
/*  SINGLE THREADED                                                                 */
/* ================================================================================ */

static void bench_narthex(void) {
    const double t0 = now_ms();
    for (nth_usize i = 0; i < OPS; ++i)
        nth_log(NTH_LOG_LEVEL_INFO, MSG);
    nth_flush();
    record("single thread", "narthex  nth_log", now_ms() - t0, OPS);
}

static void bench_narthex_fmt(void) {
    const double t0 = now_ms();
    for (nth_usize i = 0; i < OPS; ++i)
        nth_logf(NTH_LOG_LEVEL_INFO, "%s %u", MSG, (unsigned)i);
    nth_flush();
    record("single thread", "narthex  nth_logf", now_ms() - t0, OPS);
}

static void bench_stdio(const char *name, int buffered, int cached) {
    FILE *f = fopen(NULL_DEVICE, "wb");
    char *vbuf = NULL;
    Stamp st = STAMP_INIT;
    char ts[24];

    if (f == NULL)
        return;

    if (buffered) {
        vbuf = (char *)malloc(BUF_SIZE);
        setvbuf(f, vbuf, _IOFBF, BUF_SIZE);
    } else {
        setvbuf(f, NULL, _IONBF, 0);
    }

    const double t0 = now_ms();
    for (nth_usize i = 0; i < OPS; ++i) {
        if (cached)
            stamp_get(&st, ts);
        else
            stamp_get_uncached(ts);
        fprintf(f, "%s [MSG] - %s\n", ts, MSG);
    }
    fflush(f);
    record("single thread", name, now_ms() - t0, OPS);

    fclose(f);
    free(vbuf);
}

static void bench_snprintf_write(void) {
    Stamp st = STAMP_INIT;
    char line[LINE_MAX];
    char ts[24];

    const double t0 = now_ms();
    for (nth_usize i = 0; i < OPS; ++i) {
        stamp_get(&st, ts);
        const int n = snprintf(line, sizeof line, "%s [MSG] - %s\n", ts, MSG);
        raw_write(line, (nth_usize)n);
    }
    record("single thread", "snprintf + write per line", now_ms() - t0, OPS);
}

static void bench_manual_buffer(void) {
    char *buf = (char *)malloc(BUF_SIZE);
    Stamp st = STAMP_INIT;
    char line[LINE_MAX];
    char ts[24];
    nth_usize used = 0;

    if (buf == NULL)
        return;

    const double t0 = now_ms();
    for (nth_usize i = 0; i < OPS; ++i) {
        stamp_get(&st, ts);
        const nth_usize n =
            (nth_usize)snprintf(line, sizeof line, "%s [MSG] - %s\n", ts, MSG);

        if (used + n > BUF_SIZE) {
            raw_write(buf, used);
            used = 0;
        }
        memcpy(buf + used, line, n);
        used += n;
    }
    if (used != 0)
        raw_write(buf, used);
    record("single thread", "manual buffer, no locking", now_ms() - t0, OPS);

    free(buf);
}


/* ================================================================================ */
/*  MULTI THREADED                                                                  */
/* ================================================================================ */

#if NTH_PLATFORM_WINDOWS
typedef CRITICAL_SECTION BenchMutex;
typedef HANDLE           BenchThread;
typedef void (*WorkerFn)(void);

static void bench_mutex_init(BenchMutex *m)    { InitializeCriticalSection(m); }
static void bench_mutex_lock(BenchMutex *m)    { EnterCriticalSection(m); }
static void bench_mutex_unlock(BenchMutex *m)  { LeaveCriticalSection(m); }
static void bench_mutex_destroy(BenchMutex *m) { DeleteCriticalSection(m); }

static DWORD WINAPI thread_shim(LPVOID p) { ((WorkerFn)p)(); return 0; }
static BenchThread thread_start(WorkerFn fn) {
    return CreateThread(NULL, 0, thread_shim, (LPVOID)fn, 0, NULL);
}
static void thread_join(BenchThread t) {
    WaitForSingleObject(t, INFINITE);
    CloseHandle(t);
}
#else
typedef pthread_mutex_t BenchMutex;
typedef pthread_t       BenchThread;
typedef void (*WorkerFn)(void);

static void bench_mutex_init(BenchMutex *m)    { pthread_mutex_init(m, NULL); }
static void bench_mutex_lock(BenchMutex *m)    { pthread_mutex_lock(m); }
static void bench_mutex_unlock(BenchMutex *m)  { pthread_mutex_unlock(m); }
static void bench_mutex_destroy(BenchMutex *m) { pthread_mutex_destroy(m); }

static void *thread_shim(void *p) { ((WorkerFn)p)(); return NULL; }
static BenchThread thread_start(WorkerFn fn) {
    pthread_t t;
    pthread_create(&t, NULL, thread_shim, (void *)fn);
    return t;
}
static void thread_join(BenchThread t) { pthread_join(t, NULL); }
#endif

static FILE      *g_shared;
static BenchMutex g_shared_lock;

static void worker_narthex(void) {
    for (nth_usize i = 0; i < OPS_PER_TH; ++i)
        nth_log(NTH_LOG_LEVEL_INFO, MSG);
}

static void worker_stdio_mutex(void) {
    Stamp st = STAMP_INIT;
    char ts[24];

    for (nth_usize i = 0; i < OPS_PER_TH; ++i) {
        stamp_get(&st, ts);
        bench_mutex_lock(&g_shared_lock);
        fprintf(g_shared, "%s [MSG] - %s\n", ts, MSG);
        bench_mutex_unlock(&g_shared_lock);
    }
}

static void worker_stdio_plain(void) {
    Stamp st = STAMP_INIT;
    char ts[24];

    for (nth_usize i = 0; i < OPS_PER_TH; ++i) {
        stamp_get(&st, ts);
        fprintf(g_shared, "%s [MSG] - %s\n", ts, MSG);
    }
}

static double run_threaded(WorkerFn fn) {
    BenchThread t[THREADS];

    const double t0 = now_ms();
    for (nth_usize i = 0; i < THREADS; ++i)
        t[i] = thread_start(fn);
    for (nth_usize i = 0; i < THREADS; ++i)
        thread_join(t[i]);
    return now_ms() - t0;
}

static void bench_threaded(void) {
    char *vbuf = (char *)malloc(BUF_SIZE);

    double ms = run_threaded(worker_narthex);
    nth_flush();
    record("8 threads", "narthex  nth_log", ms, OPS);

    g_shared = fopen(NULL_DEVICE, "wb");
    if (g_shared == NULL) {
        free(vbuf);
        return;
    }
    setvbuf(g_shared, vbuf, _IOFBF, BUF_SIZE);
    bench_mutex_init(&g_shared_lock);

    ms = run_threaded(worker_stdio_mutex);
    fflush(g_shared);
    record("8 threads", "buffered stdio + mutex", ms, OPS);

    ms = run_threaded(worker_stdio_plain);
    fflush(g_shared);
    record("8 threads", "buffered stdio, implicit lock", ms, OPS);

    bench_mutex_destroy(&g_shared_lock);
    fclose(g_shared);
    free(vbuf);
}


/* ================================================================================ */
/*  REPORT                                                                          */
/* ================================================================================ */

static void print_results(void) {
    const char *group = NULL;
    double base = 0.0;

    printf("\nlog benchmark: %u ops per case, %u threads in the threaded group\n",
           (unsigned)OPS, (unsigned)THREADS);
    printf("all output goes to %s, results below\n", NULL_DEVICE);

    for (nth_usize i = 0; i < g_res_count; ++i) {
        if (group == NULL || strcmp(group, g_res[i].group) != 0) {
            group = g_res[i].group;
            base  = g_res[i].ms;
            printf("\n%-32s %10s %10s %10s\n", group, "total ms", "ns/op", "vs narthex");
            printf("--------------------------------------------------------------------\n");
        }
        const double ns = g_res[i].ms * 1000000.0 / (double)g_res[i].ops;
        printf("  %-30s %10.2f %10.1f %9.2fx\n",
               g_res[i].name, g_res[i].ms, ns,
               base > 0.0 ? g_res[i].ms / base : 0.0);
    }
    printf("\n");
}


int main(void) {
    NthLoggerDesc desc = {0};
    desc.buffer_size = BUF_SIZE;

    if (freopen(NULL_DEVICE, "wb", stderr) == NULL) {
        printf("could not redirect stderr\n");
        return EXIT_FAILURE;
    }
    if (nth_init_log(&desc) != NTH_RESULT_OK) {
        printf("nth_init_log failed\n");
        return EXIT_FAILURE;
    }

    bench_narthex();
    bench_narthex_fmt();
    bench_stdio("buffered stdio, cached time", 1, 1);
    bench_stdio("buffered stdio, strftime each", 1, 0);
    bench_stdio("unbuffered stdio, cached time", 0, 1);
    bench_snprintf_write();
    bench_manual_buffer();
    bench_threaded();

    nth_term_log();

    print_results();
    return EXIT_SUCCESS;
}
