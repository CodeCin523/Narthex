#include <narthex/utils/platform.h>

#if !NTH_PLATFORM_WINDOWS && !defined(_POSIX_C_SOURCE)
    #define _POSIX_C_SOURCE 200809L
#endif
#if NTH_PLATFORM_LINUX && !defined(_GNU_SOURCE)
    #define _GNU_SOURCE
#endif

#include <narthex/nth_log.h>

#include <narthex/narthex.h>
#include <narthex/utils/arch.h>
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
    #include <sched.h>
    #include <unistd.h>
    #define NULL_DEVICE "/dev/null"
#endif

#if NTH_ARCH_X86_64
    #include <x86intrin.h>
#endif




#define OPS_DEFAULT 2000000u
#define THREADS     8u
#define BUF_SIZE    (64u * 1024u)
#define LINE_MAX    256
#define WARMUP_MS   250.0


static nth_usize g_ops        = OPS_DEFAULT;
static nth_usize g_ops_per_th = OPS_DEFAULT / THREADS;

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

static const char TAG[] = " [MSG] - ";

#define MSG_LEN (sizeof MSG - 1)

static nth_usize build_line(char *out, Stamp *st, const char *msg) {
    char *p = out;

    stamp_get(st, p);
    p += 19;
    memcpy(p, TAG, sizeof TAG - 1);
    p += sizeof TAG - 1;
    memcpy(p, msg, MSG_LEN);
    p += MSG_LEN;
    *p++ = '\n';

    return (nth_usize)(p - out);
}


/* ================================================================================ */
/*  COLD SOURCE                                                                     */
/* ================================================================================ */

#define COLD_BYTES  (128u * 1024u * 1024u)
#define COLD_STRIDE 64u
#define COLD_COUNT  (COLD_BYTES / COLD_STRIDE)

static char *g_cold;

static nth_b8 cold_setup(void) {
    g_cold = (char *)malloc(COLD_BYTES);
    if (g_cold == NULL)
        return NTH_FALSE;

    for (nth_usize i = 0; i < COLD_COUNT; ++i) {
        char *p = g_cold + i * COLD_STRIDE;
        memcpy(p, MSG, MSG_LEN);
        p[0] = (char)('a' + (i % 26));
    }
    return NTH_TRUE;
}

static void cold_teardown(void) {
    free(g_cold);
    g_cold = NULL;
}

static const char *cold_at(nth_u32 *state) {
    nth_u32 x = *state;

    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;

    return g_cold + (nth_usize)(x % COLD_COUNT) * COLD_STRIDE;
}


/* ================================================================================ */
/*  SINGLE THREADED                                                                 */
/* ================================================================================ */


static void warmup(void) {
    const double t0 = now_ms();

    while (now_ms() - t0 < WARMUP_MS)
        for (nth_usize i = 0; i < 1000; ++i)
            nth_log(NTH_LOG_LEVEL_INFO, MSG);

    nth_flush();
}

static void bench_narthex(void) {
    const double t0 = now_ms();
    for (nth_usize i = 0; i < g_ops; ++i)
        nth_log(NTH_LOG_LEVEL_INFO, MSG);
    nth_flush();
    record("single thread", "narthex  nth_log", now_ms() - t0, g_ops);
}

static void bench_narthex_fmt(void) {
    const double t0 = now_ms();
    for (nth_usize i = 0; i < g_ops; ++i)
        nth_logf(NTH_LOG_LEVEL_INFO, "%s %u", MSG, (unsigned)i);
    nth_flush();
    record("single thread", "narthex  nth_logf", now_ms() - t0, g_ops);
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
    for (nth_usize i = 0; i < g_ops; ++i) {
        if (cached)
            stamp_get(&st, ts);
        else
            stamp_get_uncached(ts);
        fprintf(f, "%s [MSG] - %s\n", ts, MSG);
    }
    fflush(f);
    record("single thread", name, now_ms() - t0, g_ops);

    fclose(f);
    free(vbuf);
}

static void bench_snprintf_write(void) {
    Stamp st = STAMP_INIT;
    char line[LINE_MAX];
    char ts[24];

    const double t0 = now_ms();
    for (nth_usize i = 0; i < g_ops; ++i) {
        stamp_get(&st, ts);
        const int n = snprintf(line, sizeof line, "%s [MSG] - %s\n", ts, MSG);
        raw_write(line, (nth_usize)n);
    }
    record("single thread", "snprintf + write per line", now_ms() - t0, g_ops);
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
    for (nth_usize i = 0; i < g_ops; ++i) {
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
    record("single thread", "manual buffer, no locking", now_ms() - t0, g_ops);

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
    for (nth_usize i = 0; i < g_ops_per_th; ++i)
        nth_log(NTH_LOG_LEVEL_INFO, MSG);
}

static void worker_stdio_mutex(void) {
    Stamp st = STAMP_INIT;
    char ts[24];

    for (nth_usize i = 0; i < g_ops_per_th; ++i) {
        stamp_get(&st, ts);
        bench_mutex_lock(&g_shared_lock);
        fprintf(g_shared, "%s [MSG] - %s\n", ts, MSG);
        bench_mutex_unlock(&g_shared_lock);
    }
}

static void worker_stdio_plain(void) {
    Stamp st = STAMP_INIT;
    char ts[24];

    for (nth_usize i = 0; i < g_ops_per_th; ++i) {
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
    
    const nth_usize total = g_ops_per_th * THREADS;

    double ms = run_threaded(worker_narthex);
    nth_flush();
    record("8 threads", "narthex  nth_log", ms, total);

    g_shared = fopen(NULL_DEVICE, "wb");
    if (g_shared == NULL) {
        free(vbuf);
        return;
    }
    setvbuf(g_shared, vbuf, _IOFBF, BUF_SIZE);
    bench_mutex_init(&g_shared_lock);

    ms = run_threaded(worker_stdio_mutex);
    fflush(g_shared);
    record("8 threads", "buffered stdio + mutex", ms, total);

    ms = run_threaded(worker_stdio_plain);
    fflush(g_shared);
    record("8 threads", "buffered stdio, implicit lock", ms, total);

    bench_mutex_destroy(&g_shared_lock);
    fclose(g_shared);
    free(vbuf);
}


/* ================================================================================ */
/*  MESSAGE SOURCE                                                                  */
/* ================================================================================ */

static void bench_source(void) {
    nth_u32 rnd = 2463534242u;
    Stamp   st  = STAMP_INIT;
    char    line[LINE_MAX];
    char   *vbuf;
    FILE   *f;
    double  t0;

    t0 = now_ms();
    for (nth_usize i = 0; i < g_ops; ++i) {
        (void)cold_at(&rnd);
        nth_logn(NTH_LOG_LEVEL_INFO, MSG, MSG_LEN);
    }
    nth_flush();
    record("message source", "narthex  nth_logn, hot", now_ms() - t0, g_ops);

    t0 = now_ms();
    for (nth_usize i = 0; i < g_ops; ++i)
        nth_logn(NTH_LOG_LEVEL_INFO, cold_at(&rnd), MSG_LEN);
    nth_flush();
    record("message source", "narthex  nth_logn, cold", now_ms() - t0, g_ops);

    f = fopen(NULL_DEVICE, "wb");
    if (f == NULL)
        return;

    vbuf = (char *)malloc(BUF_SIZE);
    setvbuf(f, vbuf, _IOFBF, BUF_SIZE);

    t0 = now_ms();
    for (nth_usize i = 0; i < g_ops; ++i) {
        (void)cold_at(&rnd);
        fwrite(line, 1, build_line(line, &st, MSG), f);
    }
    fflush(f);
    record("message source", "FILE* fwrite, hot", now_ms() - t0, g_ops);

    t0 = now_ms();
    for (nth_usize i = 0; i < g_ops; ++i)
        fwrite(line, 1, build_line(line, &st, cold_at(&rnd)), f);
    fflush(f);
    record("message source", "FILE* fwrite, cold", now_ms() - t0, g_ops);

    fclose(f);
    free(vbuf);
}


/* ================================================================================ */
/*  LATENCY                                                                         */
/* ================================================================================ */

#define LAT_SAMPLES 200000u
#define LAT_MAX_TH  64u

typedef struct {
    const char *name;
    nth_usize   threads;
    double      p50;
    double      p99;
    double      p999;
    double      max;
} LatResult;

static LatResult  g_lat[16];
static nth_usize  g_lat_count;
static nth_u32   *g_lat_sample[LAT_MAX_TH];
static double     g_tick_ns = 1.0;
static nth_u64    g_lat_probe;
static int        g_core[LAT_MAX_TH];
static int        g_core_count;

static nth_u64 lat_tick(void) {
#if NTH_ARCH_X86_64
    unsigned aux;
    nth_u64  t;

    _mm_lfence();
    t = (nth_u64)__rdtscp(&aux);
    _mm_lfence();
    return t;
#elif NTH_PLATFORM_WINDOWS
    LARGE_INTEGER c;
    QueryPerformanceCounter(&c);
    return (nth_u64)c.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (nth_u64)ts.tv_sec * 1000000000ull + (nth_u64)ts.tv_nsec;
#endif
}

static void lat_calibrate(void) {
#if NTH_ARCH_X86_64
    const double  t0 = now_ms();
    const nth_u64 c0 = lat_tick();

    while (now_ms() - t0 < 100.0)
        ;

    const double  el = now_ms() - t0;
    const nth_u64 c1 = lat_tick();
    g_tick_ns = el * 1000000.0 / (double)(c1 - c0);
#elif NTH_PLATFORM_WINDOWS
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    g_tick_ns = 1000000000.0 / (double)f.QuadPart;
#endif
}

static void lat_find_cores(void) {
#if NTH_PLATFORM_LINUX
    char path[128], buf[256];

    for (int cpu = 0; cpu < 1024 && g_core_count < (int)LAT_MAX_TH; ++cpu) {
        snprintf(path, sizeof path,
                 "/sys/devices/system/cpu/cpu%d/topology/thread_siblings_list", cpu);

        FILE *f = fopen(path, "r");
        if (f == NULL)
            continue;
        if (fgets(buf, sizeof buf, f) != NULL && atoi(buf) == cpu)
            g_core[g_core_count++] = cpu;
        fclose(f);
    }
#endif
    if (g_core_count == 0)
        for (int i = 0; i < (int)LAT_MAX_TH; ++i)
            g_core[g_core_count++] = i;
}

static void lat_pin(unsigned slot) {
    const int cpu = g_core[slot % (unsigned)g_core_count];

#if NTH_PLATFORM_WINDOWS
    SetThreadAffinityMask(GetCurrentThread(), (DWORD_PTR)1 << (cpu % 64));
#elif defined(__linux__)
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu, &set);
    pthread_setaffinity_np(pthread_self(), sizeof set, &set);
#else
    (void)cpu;
#endif
}

typedef void (*LatOp)(Stamp *st, char *line, const char *msg);

static LatOp  g_lat_op;
static nth_b8 g_lat_cold;

static void lat_op_narthex(Stamp *st, char *line, const char *msg) {
    (void)st;
    (void)line;
    nth_logn(NTH_LOG_LEVEL_INFO, msg, MSG_LEN);
}

static void lat_op_fwrite(Stamp *st, char *line, const char *msg) {
    const nth_usize n = build_line(line, st, msg);
    fwrite(line, 1, n, g_shared);
}

static void lat_worker(unsigned id) {
    Stamp    st  = STAMP_INIT;
    nth_u32  rnd = 2463534242u + (nth_u32)id * 7919u;
    char     line[LINE_MAX];
    nth_u32 *out = g_lat_sample[id];

    lat_pin(id);

    for (nth_usize i = 0; i < LAT_SAMPLES; ++i) {
        const char *msg = g_lat_cold ? cold_at(&rnd) : MSG;

        const nth_u64 a = lat_tick();
        g_lat_op(&st, line, msg);
        const nth_u64 b = lat_tick();

        out[i] = (nth_u32)(b - a);
    }
}

typedef struct {
    unsigned id;
} LatArg;

#if NTH_PLATFORM_WINDOWS
static DWORD WINAPI lat_shim(LPVOID p) {
    lat_worker(((LatArg *)p)->id);
    return 0;
}
static BenchThread lat_start(LatArg *a) {
    return CreateThread(NULL, 0, lat_shim, (LPVOID)a, 0, NULL);
}
#else
static void *lat_shim(void *p) {
    lat_worker(((LatArg *)p)->id);
    return NULL;
}
static BenchThread lat_start(LatArg *a) {
    pthread_t t;
    pthread_create(&t, NULL, lat_shim, (void *)a);
    return t;
}
#endif

static int lat_cmp(const void *a, const void *b) {
    const nth_u32 x = *(const nth_u32 *)a, y = *(const nth_u32 *)b;
    return x < y ? -1 : (x > y ? 1 : 0);
}

static double lat_ns(nth_u32 raw) {
    const double net = (double)raw - (double)g_lat_probe;
    return (net < 0.0 ? 0.0 : net) * g_tick_ns;
}

static double lat_pick(const nth_u32 *sorted, nth_usize n, double p) {
    nth_usize i = (nth_usize)(p * (double)n);
    return lat_ns(sorted[i >= n ? n - 1 : i]);
}

static void lat_run(const char *name, LatOp op, nth_usize threads) {
    BenchThread th[LAT_MAX_TH];
    LatArg      arg[LAT_MAX_TH];

    g_lat_op = op;

    for (nth_usize i = 0; i < threads; ++i) {
        arg[i].id = (unsigned)i;
        g_lat_sample[i] = (nth_u32 *)malloc(LAT_SAMPLES * sizeof **g_lat_sample);
        if (g_lat_sample[i] == NULL)
            return;
    }

    if (threads == 1) {
        lat_worker(0);
    } else {
        for (nth_usize i = 0; i < threads; ++i)
            th[i] = lat_start(&arg[i]);
        for (nth_usize i = 0; i < threads; ++i)
            thread_join(th[i]);
    }

    const nth_usize total = threads * LAT_SAMPLES;
    nth_u32 *all = (nth_u32 *)malloc(total * sizeof *all);

    if (all != NULL) {
        for (nth_usize i = 0; i < threads; ++i)
            memcpy(all + i * LAT_SAMPLES, g_lat_sample[i],
                   LAT_SAMPLES * sizeof *all);

        qsort(all, total, sizeof *all, lat_cmp);

        g_lat[g_lat_count].name    = name;
        g_lat[g_lat_count].threads = threads;
        g_lat[g_lat_count].p50     = lat_pick(all, total, 0.50);
        g_lat[g_lat_count].p99     = lat_pick(all, total, 0.99);
        g_lat[g_lat_count].p999    = lat_pick(all, total, 0.999);
        g_lat[g_lat_count].max     = lat_ns(all[total - 1]);
        g_lat_count++;

        free(all);
    }

    for (nth_usize i = 0; i < threads; ++i)
        free(g_lat_sample[i]);
}

static void bench_latency(void) {
    char *vbuf = (char *)malloc(BUF_SIZE);

    lat_find_cores();
    lat_calibrate();

    g_lat_probe = ~(nth_u64)0;
    for (nth_usize i = 0; i < 10000; ++i) {
        const nth_u64 a = lat_tick();
        const nth_u64 b = lat_tick();
        if (b - a < g_lat_probe)
            g_lat_probe = b - a;
    }

    g_lat_cold = NTH_FALSE;
    lat_run("narthex  nth_logn hot", lat_op_narthex, 1);
    lat_run("narthex  nth_logn hot", lat_op_narthex, THREADS);
    g_lat_cold = NTH_TRUE;
    lat_run("narthex  nth_logn cold", lat_op_narthex, 1);
    lat_run("narthex  nth_logn cold", lat_op_narthex, THREADS);
    nth_flush();

    g_shared = fopen(NULL_DEVICE, "wb");
    if (g_shared == NULL) {
        free(vbuf);
        return;
    }
    setvbuf(g_shared, vbuf, _IOFBF, BUF_SIZE);

    g_lat_cold = NTH_FALSE;
    lat_run("FILE* fwrite hot", lat_op_fwrite, 1);
    lat_run("FILE* fwrite hot", lat_op_fwrite, THREADS);
    g_lat_cold = NTH_TRUE;
    lat_run("FILE* fwrite cold", lat_op_fwrite, 1);
    lat_run("FILE* fwrite cold", lat_op_fwrite, THREADS);
    fflush(g_shared);

    fclose(g_shared);
    g_shared = NULL;
    free(vbuf);
}


/* ================================================================================ */
/*  REPORT                                                                          */
/* ================================================================================ */

static void print_results(void) {
    const char *group = NULL;
    double base = 0.0;

    printf("\nlog benchmark: %u ops per case, %u threads in the threaded group\n",
           (unsigned)g_ops, (unsigned)THREADS);
    printf("all output goes to %s, after a %.0f ms warmup\n", NULL_DEVICE, WARMUP_MS);

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

static void print_latency(void) {
    printf("\nper call latency, %u samples per thread, pinned one per physical core\n",
           (unsigned)LAT_SAMPLES);
    printf("serialised probe costs %.1f ns and is subtracted\n",
           (double)g_lat_probe * g_tick_ns);

    printf("\n%-24s %8s %9s %9s %10s %11s\n",
           "", "threads", "p50 ns", "p99 ns", "p99.9 ns", "max ns");
    printf("--------------------------------------------------------------------------\n");

    for (nth_usize i = 0; i < g_lat_count; ++i)
        printf("  %-22s %8u %9.1f %9.1f %10.1f %11.1f\n",
               g_lat[i].name, (unsigned)g_lat[i].threads,
               g_lat[i].p50, g_lat[i].p99, g_lat[i].p999, g_lat[i].max);
    printf("\n");
}


int main(int argc, char **argv) {
    NthCoreDesc desc = {0};
    desc.logger.buffer_size = BUF_SIZE;

    if (argc > 1) {
        const long ops = strtol(argv[1], NULL, 10);

        if (ops < (long)THREADS) {
            printf("usage: %s [ops per case, at least %u]\n", argv[0], (unsigned)THREADS);
            return EXIT_FAILURE;
        }
        g_ops        = (nth_usize)ops;
        g_ops_per_th = g_ops / THREADS;
    }

    if (freopen(NULL_DEVICE, "wb", stderr) == NULL) {
        printf("could not redirect stderr\n");
        return EXIT_FAILURE;
    }
    if (nth_init(&desc) != NTH_RESULT_OK) {
        printf("nth_init failed\n");
        return EXIT_FAILURE;
    }

    warmup();

    bench_narthex();
    bench_narthex_fmt();
    bench_stdio("buffered stdio, cached time", 1, 1);
    bench_stdio("buffered stdio, strftime each", 1, 0);
    bench_stdio("unbuffered stdio, cached time", 0, 1);
    bench_snprintf_write();
    bench_manual_buffer();
    bench_threaded();

    if (!cold_setup()) {
        printf("could not allocate the cold message ring\n");
        return EXIT_FAILURE;
    }
    bench_source();
    bench_latency();
    cold_teardown();

    nth_term();

    print_results();
    print_latency();
    return EXIT_SUCCESS;
}
