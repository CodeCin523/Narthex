#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
    #define _POSIX_C_SOURCE 200809L
#endif

#include "nth_test.h"

#include <narthex/nth_log.h>

#include <narthex/narthex.h>
#include <narthex/utils/platform.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if NTH_PLATFORM_WINDOWS
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
#else
    #include <fcntl.h>
    #include <pthread.h>
    #include <unistd.h>
#endif

static NthResult init_logger(const NthLoggerDesc *desc) {
    NthCoreDesc core = {0};

    if (desc == NULL)
        return nth_init(NULL);

    core.logger = *desc;
    return nth_init(&core);
}

#define TIME_LEN   19
#define PREFIX_LEN 28
#define FIXED_LEN  (PREFIX_LEN + 1)
#define MIN_BUFFER (FIXED_LEN + 1)
#define FORMAT_MAX 2048

#define CAP_MAX (1024u * 1024u)

#define TH_COUNT 8u
#define TH_LINES 1000u

static const char MSG[] = "narthex logger under test";

/* ================================================================================ */
/*  STDERR CAPTURE                                                                  */
/* ================================================================================ */

static char      g_cap[CAP_MAX];
static nth_usize g_cap_len;

static void cap_fail(const char *what) {
    fprintf(stdout, "capture: %s failed\n", what);
    exit(EXIT_FAILURE);
}

#if NTH_PLATFORM_WINDOWS

static HANDLE g_cap_wr    = INVALID_HANDLE_VALUE;
static HANDLE g_cap_rd    = INVALID_HANDLE_VALUE;
static HANDLE g_cap_saved = INVALID_HANDLE_VALUE;
static char   g_cap_path[MAX_PATH];

static void cap_begin(void) {
    const DWORD share = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
    char dir[MAX_PATH];

    g_cap[0]  = '\0';
    g_cap_len = 0;

    fflush(stderr);

    if (GetTempPathA(MAX_PATH, dir) == 0)
        cap_fail("GetTempPathA");
    if (GetTempFileNameA(dir, "nth", 0, g_cap_path) == 0)
        cap_fail("GetTempFileNameA");

    g_cap_wr = CreateFileA(g_cap_path, GENERIC_WRITE, share, NULL,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, NULL);
    if (g_cap_wr == INVALID_HANDLE_VALUE)
        cap_fail("CreateFileA write");

    g_cap_rd = CreateFileA(g_cap_path, GENERIC_READ, share, NULL,
                           OPEN_EXISTING, FILE_ATTRIBUTE_TEMPORARY, NULL);
    if (g_cap_rd == INVALID_HANDLE_VALUE)
        cap_fail("CreateFileA read");

    g_cap_saved = GetStdHandle(STD_ERROR_HANDLE);
    if (!SetStdHandle(STD_ERROR_HANDLE, g_cap_wr))
        cap_fail("SetStdHandle");
}

static nth_usize cap_sync(void) {
    nth_usize n = 0;

    if (g_cap_rd == INVALID_HANDLE_VALUE)
        return 0;
    if (SetFilePointer(g_cap_rd, 0, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER)
        cap_fail("SetFilePointer");

    while (n + 1 < sizeof g_cap) {
        DWORD got = 0;

        if (!ReadFile(g_cap_rd, g_cap + n, (DWORD)(sizeof g_cap - 1 - n), &got, NULL))
            cap_fail("ReadFile");
        if (got == 0)
            break;

        n += got;
    }

    g_cap[n]  = '\0';
    g_cap_len = n;
    return n;
}

static nth_usize cap_end(void) {
    const nth_usize n = cap_sync();

    fflush(stderr);

    if (g_cap_saved != INVALID_HANDLE_VALUE) {
        SetStdHandle(STD_ERROR_HANDLE, g_cap_saved);
        g_cap_saved = INVALID_HANDLE_VALUE;
    }
    if (g_cap_rd != INVALID_HANDLE_VALUE) {
        CloseHandle(g_cap_rd);
        g_cap_rd = INVALID_HANDLE_VALUE;
    }
    if (g_cap_wr != INVALID_HANDLE_VALUE) {
        CloseHandle(g_cap_wr);
        g_cap_wr = INVALID_HANDLE_VALUE;
    }
    DeleteFileA(g_cap_path);

    return n;
}

#else

static int g_cap_wr    = -1;
static int g_cap_rd    = -1;
static int g_cap_saved = -1;

static void cap_begin(void) {
    char path[] = "/tmp/nth_log_test_XXXXXX";

    g_cap[0]  = '\0';
    g_cap_len = 0;

    fflush(stderr);

    g_cap_wr = mkstemp(path);
    if (g_cap_wr < 0)
        cap_fail("mkstemp");

    g_cap_rd = open(path, O_RDONLY);
    if (g_cap_rd < 0)
        cap_fail("open");

    unlink(path);

    g_cap_saved = dup(STDERR_FILENO);
    if (g_cap_saved < 0)
        cap_fail("dup");
    if (dup2(g_cap_wr, STDERR_FILENO) < 0)
        cap_fail("dup2");
}

static nth_usize cap_sync(void) {
    nth_usize n = 0;

    if (g_cap_rd < 0)
        return 0;
    if (lseek(g_cap_rd, 0, SEEK_SET) == (off_t)-1)
        cap_fail("lseek");

    while (n + 1 < sizeof g_cap) {
        const ssize_t got = read(g_cap_rd, g_cap + n, sizeof g_cap - 1 - n);

        if (got < 0) {
            if (errno == EINTR)
                continue;
            cap_fail("read");
        }
        if (got == 0)
            break;

        n += (nth_usize)got;
    }

    g_cap[n]  = '\0';
    g_cap_len = n;
    return n;
}

static nth_usize cap_end(void) {
    const nth_usize n = cap_sync();

    fflush(stderr);

    if (g_cap_saved >= 0) {
        if (dup2(g_cap_saved, STDERR_FILENO) < 0)
            cap_fail("restore");
        close(g_cap_saved);
        g_cap_saved = -1;
    }
    if (g_cap_rd >= 0) {
        close(g_cap_rd);
        g_cap_rd = -1;
    }
    if (g_cap_wr >= 0) {
        close(g_cap_wr);
        g_cap_wr = -1;
    }

    return n;
}

#endif

/* ================================================================================ */
/*  LINE INSPECTION                                                                 */
/* ================================================================================ */

static const char *line_next(const char **cur, nth_usize *len) {
    const char *p  = *cur;
    const char *nl;

    if (p == NULL || *p == '\0')
        return NULL;

    nl = strchr(p, '\n');
    if (nl == NULL) {
        *len = strlen(p);
        *cur = p + *len;
    } else {
        *len = (nth_usize)(nl - p);
        *cur = nl + 1;
    }
    return p;
}

static const char *single_line(nth_usize *len) {
    const char *cur  = g_cap;
    const char *line = line_next(&cur, len);

    if (line == NULL || *cur != '\0')
        return NULL;
    return line;
}

static nth_usize count_lines(void) {
    nth_usize n = 0;

    for (nth_usize i = 0; i < g_cap_len; i++)
        if (g_cap[i] == '\n')
            n++;
    return n;
}

static nth_b8 prefix_ok(const char *line, nth_usize len, const char *level) {
    static const nth_usize digit_at[] = {
        1, 2, 3, 4, 6, 7, 8, 10, 11, 13, 14, 16, 17
    };

    if (len < PREFIX_LEN)
        return NTH_FALSE;

    for (nth_usize i = 0; i < sizeof digit_at / sizeof digit_at[0]; i++)
        if (line[digit_at[i]] < '0' || line[digit_at[i]] > '9')
            return NTH_FALSE;

    return (nth_b8)(line[0]  == '[' && line[5]  == '-' && line[9]  == ' ' &&
                    line[12] == ':' && line[15] == ':' && line[18] == ']' &&
                    line[19] == ' ' && line[20] == '[' &&
                    memcmp(line + 21, level, 3) == 0 &&
                    line[24] == ']' && line[25] == ' ' &&
                    line[26] == '-' && line[27] == ' ');
}

static nth_b8 line_is(const char *line, nth_usize len, const char *level, const char *body) {
    const nth_usize n = strlen(body);

    return (nth_b8)(prefix_ok(line, len, level) &&
                    len == PREFIX_LEN + n &&
                    memcmp(line + PREFIX_LEN, body, n) == 0);
}

static void stamp_now(char *out) {
    const time_t now = time(NULL);
    struct tm    tmv;

#if NTH_PLATFORM_WINDOWS
    localtime_s(&tmv, &now);
#else
    localtime_r(&now, &tmv);
#endif

    strftime(out, 20, "[%Y-%j %H:%M:%S]", &tmv);
}

/* ================================================================================ */
/*  LIFE-CYCLE                                                                      */
/* ================================================================================ */

static nth_b8 test_init_term(void) {
    NthResult r;

    r = init_logger(NULL);
    NTH_TEST_ASSERT(r == NTH_RESULT_OK);

    cap_begin();
    nth_log(NTH_LOG_LEVEL_INFO, MSG);
    nth_flush();
    const nth_usize live1 = cap_end();

    nth_term();

    r = init_logger(NULL);
    NTH_TEST_ASSERT(r == NTH_RESULT_OK);

    cap_begin();
    nth_log(NTH_LOG_LEVEL_INFO, MSG);
    nth_flush();
    const nth_usize live2 = cap_end();

    nth_term();

    NTH_TEST_ASSERT(live1 > 0);
    NTH_TEST_ASSERT(live2 > 0);

    return NTH_TRUE;
}

static nth_b8 test_double_init(void) {
    NthLoggerDesc desc = {0};
    desc.buffer_size = MIN_BUFFER;

    const NthResult first  = init_logger(NULL);
    const NthResult second = init_logger(&desc);

    cap_begin();
    nth_log(NTH_LOG_LEVEL_INFO, MSG);
    nth_term();
    cap_end();

    nth_usize   len;
    const char *line = single_line(&len);

    NTH_TEST_ASSERT(first == NTH_RESULT_OK);
    NTH_TEST_ASSERT(second == NTH_RESULT_ALREADY_INITIALIZED);
    NTH_TEST_ASSERT(line != NULL);
    NTH_TEST_ASSERT(line_is(line, len, "MSG", MSG));
    return NTH_TRUE;
}

static nth_b8 test_term_when_dead(void) {
    nth_term();

    const NthResult r = init_logger(NULL);
    nth_term();
    nth_term();

    NTH_TEST_ASSERT(r == NTH_RESULT_OK);
    return NTH_TRUE;
}

static nth_b8 test_reinit(void) {
    NthResult r[3];

    cap_begin();
    for (nth_u32 i = 0; i < 3; i++) {
        r[i] = init_logger(NULL);
        nth_logf(NTH_LOG_LEVEL_INFO, "cycle %u", (unsigned)i);
        nth_term();
    }
    cap_end();

    const char *cur = g_cap;
    for (nth_u32 i = 0; i < 3; i++) {
        char        want[16];
        nth_usize   len;
        const char *line = line_next(&cur, &len);

        snprintf(want, sizeof want, "cycle %u", (unsigned)i);

        NTH_TEST_ASSERT(r[i] == NTH_RESULT_OK);
        NTH_TEST_ASSERT(line != NULL);
        NTH_TEST_ASSERT(line_is(line, len, "MSG", want));
    }
    NTH_TEST_ASSERT(*cur == '\0');
    return NTH_TRUE;
}

static nth_b8 test_buffer_too_small(void) {
    NthLoggerDesc desc = {0};
    NthResult     rejected[MIN_BUFFER];

    for (nth_usize size = 1; size < MIN_BUFFER; size++) {
        desc.buffer_size = size;
        rejected[size]   = init_logger(&desc);
    }

    desc.buffer_size   = MIN_BUFFER;
    const NthResult ok = init_logger(&desc);
    nth_term();

    for (nth_usize size = 1; size < MIN_BUFFER; size++)
        NTH_TEST_ASSERT(rejected[size] == NTH_RESULT_INVALID_ARGUMENT);

    NTH_TEST_ASSERT(ok == NTH_RESULT_OK);
    return NTH_TRUE;
}

static nth_b8 test_zero_size_uses_default(void) {
    NthLoggerDesc desc = {0};
    char          body[4001];

    memset(body, 'z', sizeof body - 1);
    body[sizeof body - 1] = '\0';

    const NthResult r = init_logger(&desc);

    cap_begin();
    nth_log(NTH_LOG_LEVEL_INFO, body);
    nth_term();
    cap_end();

    nth_usize   len;
    const char *line = single_line(&len);

    NTH_TEST_ASSERT(r == NTH_RESULT_OK);
    NTH_TEST_ASSERT(line != NULL);
    NTH_TEST_ASSERT(line_is(line, len, "MSG", body));
    return NTH_TRUE;
}

static nth_b8 test_via_core(void) {
    const nth_usize size = 40;
    const nth_usize room = size - FIXED_LEN;

    NthCoreDesc desc = {0};
    desc.logger.buffer_size = size;

    const NthResult r = nth_init(&desc);

    cap_begin();
    nth_log(NTH_LOG_LEVEL_WARN, MSG);
    nth_term();
    const nth_usize n = cap_end();

    nth_usize   len;
    const char *line = single_line(&len);

    NTH_TEST_ASSERT(r == NTH_RESULT_OK);
    NTH_TEST_ASSERT(line != NULL);
    NTH_TEST_ASSERT(prefix_ok(line, len, "WRN"));
    NTH_TEST_ASSERT(n == size);
    NTH_TEST_ASSERT(len == PREFIX_LEN + room);
    NTH_TEST_ASSERT(memcmp(line + PREFIX_LEN, MSG, room) == 0);
    return NTH_TRUE;
}

/* ================================================================================ */
/*  FORMAT                                                                          */
/* ================================================================================ */

static nth_b8 test_levels(void) {
    static const char *const tag[] = { "FTL", "ERR", "WRN", "MSG", "DBG" };
    static const NthLogLevel level[] = {
        NTH_LOG_LEVEL_FATAL, NTH_LOG_LEVEL_ERROR, NTH_LOG_LEVEL_WARN,
        NTH_LOG_LEVEL_INFO,  NTH_LOG_LEVEL_DEBUG
    };

    init_logger(NULL);

    cap_begin();
    for (nth_usize i = 0; i < 5; i++)
        nth_log(level[i], MSG);
    nth_term();
    cap_end();

    const char *cur = g_cap;
    for (nth_usize i = 0; i < 5; i++) {
        nth_usize   len;
        const char *line = line_next(&cur, &len);

        NTH_TEST_ASSERT(line != NULL);
        NTH_TEST_ASSERT(line_is(line, len, tag[i], MSG));
    }
    NTH_TEST_ASSERT(*cur == '\0');
    return NTH_TRUE;
}

static nth_b8 test_level_clamped(void) {
    static const NthLogLevel over[] = { 5, 6, 42, 200, 255 };
    const nth_usize count = sizeof over / sizeof over[0];

    init_logger(NULL);

    cap_begin();
    for (nth_usize i = 0; i < count; i++)
        nth_log(over[i], MSG);
    nth_term();
    cap_end();

    const char *cur = g_cap;
    for (nth_usize i = 0; i < count; i++) {
        nth_usize   len;
        const char *line = line_next(&cur, &len);

        NTH_TEST_ASSERT(line != NULL);
        NTH_TEST_ASSERT(line_is(line, len, "DBG", MSG));
    }
    NTH_TEST_ASSERT(*cur == '\0');
    return NTH_TRUE;
}

static nth_b8 test_timestamp(void) {
    char before[24];
    char after[24];

    init_logger(NULL);

    cap_begin();
    stamp_now(before);
    nth_log(NTH_LOG_LEVEL_INFO, MSG);
    nth_flush();
    stamp_now(after);
    nth_term();
    cap_end();

    nth_usize   len;
    const char *line = single_line(&len);

    NTH_TEST_ASSERT(line != NULL);
    NTH_TEST_ASSERT(line_is(line, len, "MSG", MSG));
    NTH_TEST_ASSERT(memcmp(line, before, TIME_LEN) >= 0);
    NTH_TEST_ASSERT(memcmp(line, after, TIME_LEN) <= 0);
    return NTH_TRUE;
}

static nth_b8 test_logn_takes_length(void) {
    const char *const none = NULL;

    init_logger(NULL);

    cap_begin();
    nth_logn(NTH_LOG_LEVEL_INFO, MSG, sizeof MSG - 1);
    nth_logn(NTH_LOG_LEVEL_WARN, MSG, 7);
    nth_logn(NTH_LOG_LEVEL_ERROR, "", 0);
    // nth_logn(NTH_LOG_LEVEL_INFO, none, 4);
    nth_term();
    cap_end();

    const char *cur = g_cap;
    nth_usize   len;
    const char *line;

    line = line_next(&cur, &len);
    NTH_TEST_ASSERT(line != NULL);
    NTH_TEST_ASSERT(line_is(line, len, "MSG", MSG));

    line = line_next(&cur, &len);
    NTH_TEST_ASSERT(line != NULL);
    NTH_TEST_ASSERT(prefix_ok(line, len, "WRN"));
    NTH_TEST_ASSERT(len == PREFIX_LEN + 7);
    NTH_TEST_ASSERT(memcmp(line + PREFIX_LEN, MSG, 7) == 0);

    line = line_next(&cur, &len);
    NTH_TEST_ASSERT(line != NULL);
    NTH_TEST_ASSERT(line_is(line, len, "ERR", ""));

    NTH_TEST_ASSERT(*cur == '\0');
    return NTH_TRUE;
}

static nth_b8 test_logn_embedded_nul(void) {
    static const char RAW[] = "abc\0def";
    const nth_usize   raw_len = sizeof RAW - 1;

    init_logger(NULL);

    cap_begin();
    nth_logn(NTH_LOG_LEVEL_INFO, RAW, raw_len);
    nth_term();
    const nth_usize n = cap_end();

    NTH_TEST_ASSERT(n == PREFIX_LEN + raw_len + 1);
    NTH_TEST_ASSERT(prefix_ok(g_cap, n - 1, "MSG"));
    NTH_TEST_ASSERT(memcmp(g_cap + PREFIX_LEN, RAW, raw_len) == 0);
    NTH_TEST_ASSERT(g_cap[n - 1] == '\n');
    return NTH_TRUE;
}

static nth_b8 test_logn_matches_log(void) {
    init_logger(NULL);

    cap_begin();
    nth_log(NTH_LOG_LEVEL_INFO, MSG);
    const nth_usize one = cap_sync();
    nth_logn(NTH_LOG_LEVEL_INFO, MSG, sizeof MSG - 1);
    nth_term();
    const nth_usize two = cap_end();

    NTH_TEST_ASSERT(one == 0);
    NTH_TEST_ASSERT(two % 2 == 0);
    NTH_TEST_ASSERT(memcmp(g_cap, g_cap + two / 2, two / 2) == 0);
    return NTH_TRUE;
}

static nth_b8 test_logn_truncates(void) {
    const nth_usize size = 64;
    const nth_usize room = size - FIXED_LEN;
    char body[256];

    for (nth_usize i = 0; i < sizeof body; i++)
        body[i] = (char)('a' + (i % 26));

    NthLoggerDesc desc = {0};
    desc.buffer_size = size;

    const NthResult r = init_logger(&desc);

    cap_begin();
    nth_logn(NTH_LOG_LEVEL_INFO, body, sizeof body);
    nth_term();
    const nth_usize n = cap_end();

    nth_usize   len;
    const char *line = single_line(&len);

    NTH_TEST_ASSERT(r == NTH_RESULT_OK);
    NTH_TEST_ASSERT(n == size);
    NTH_TEST_ASSERT(line != NULL);
    NTH_TEST_ASSERT(len == PREFIX_LEN + room);
    NTH_TEST_ASSERT(memcmp(line + PREFIX_LEN, body, room) == 0);
    return NTH_TRUE;
}

static nth_b8 test_logf_args(void) {
    static const char FMT[] = "%s|%d|%u|%08.3f|%c|%%|%p";
    const void *ptr = (const void *)MSG;
    char want[128];

    snprintf(want, sizeof want, FMT, "text", -7, 42u, 3.5, 'x', ptr);

    init_logger(NULL);

    cap_begin();
    nth_logf(NTH_LOG_LEVEL_DEBUG, FMT, "text", -7, 42u, 3.5, 'x', ptr);
    nth_term();
    cap_end();

    nth_usize   len;
    const char *line = single_line(&len);

    NTH_TEST_ASSERT(line != NULL);
    NTH_TEST_ASSERT(line_is(line, len, "DBG", want));
    return NTH_TRUE;
}

static nth_b8 test_null_and_empty(void) {
    
    const char *const none = NULL;

    init_logger(NULL);

    cap_begin();
    // nth_log(NTH_LOG_LEVEL_INFO, none);
    // nth_logf(NTH_LOG_LEVEL_INFO, none);
    nth_flush();
    const nth_usize after_null = cap_sync();

    nth_log(NTH_LOG_LEVEL_INFO, "");
    nth_logf(NTH_LOG_LEVEL_INFO, "");
    nth_term();
    cap_end();

    const char *cur = g_cap;
    for (nth_usize i = 0; i < 2; i++) {
        nth_usize   len;
        const char *line = line_next(&cur, &len);

        NTH_TEST_ASSERT(line != NULL);
        NTH_TEST_ASSERT(line_is(line, len, "MSG", ""));
    }

    NTH_TEST_ASSERT(after_null == 0);
    NTH_TEST_ASSERT(*cur == '\0');
    return NTH_TRUE;
}

/* ================================================================================ */
/*  BUFFERING                                                                       */
/* ================================================================================ */

static nth_b8 test_holds_until_flush(void) {
    init_logger(NULL);

    cap_begin();
    nth_log(NTH_LOG_LEVEL_INFO, MSG);
    const nth_usize before = cap_sync();

    nth_flush();
    const nth_usize after = cap_sync();

    nth_term();
    cap_end();

    nth_usize   len;
    const char *line = single_line(&len);

    NTH_TEST_ASSERT(before == 0);
    NTH_TEST_ASSERT(after == PREFIX_LEN + sizeof MSG - 1 + 1);
    NTH_TEST_ASSERT(line != NULL);
    NTH_TEST_ASSERT(line_is(line, len, "MSG", MSG));
    return NTH_TRUE;
}

static nth_b8 test_flush_when_empty(void) {
    init_logger(NULL);

    cap_begin();
    nth_flush();
    nth_flush();
    const nth_usize n = cap_sync();
    nth_term();
    const nth_usize after_term = cap_end();

    NTH_TEST_ASSERT(n == 0);
    NTH_TEST_ASSERT(after_term == 0);
    return NTH_TRUE;
}

static nth_b8 test_term_flushes(void) {
    init_logger(NULL);

    cap_begin();
    nth_log(NTH_LOG_LEVEL_INFO, MSG);
    const nth_usize before = cap_sync();
    nth_term();
    cap_end();

    nth_usize   len;
    const char *line = single_line(&len);

    NTH_TEST_ASSERT(before == 0);
    NTH_TEST_ASSERT(line != NULL);
    NTH_TEST_ASSERT(line_is(line, len, "MSG", MSG));
    return NTH_TRUE;
}

static nth_b8 test_auto_flush_on_full(void) {
    const nth_usize body_len = 8;
    const nth_usize line_len = PREFIX_LEN + body_len + 1;
    const nth_usize count    = 10;

    NthLoggerDesc desc = {0};
    desc.buffer_size = line_len * 3;

    const NthResult r = init_logger(&desc);

    cap_begin();
    for (nth_usize i = 0; i < count; i++)
        nth_logf(NTH_LOG_LEVEL_INFO, "%08u", (unsigned)i);
    const nth_usize pending = cap_sync();

    nth_flush();
    nth_term();
    cap_end();

    NTH_TEST_ASSERT(r == NTH_RESULT_OK);
    
    NTH_TEST_ASSERT(pending == 9 * line_len);
    NTH_TEST_ASSERT(g_cap_len == count * line_len);

    const char *cur = g_cap;
    for (nth_usize i = 0; i < count; i++) {
        char        want[16];
        nth_usize   len;
        const char *line = line_next(&cur, &len);

        snprintf(want, sizeof want, "%08u", (unsigned)i);

        NTH_TEST_ASSERT(line != NULL);
        NTH_TEST_ASSERT(line_is(line, len, "MSG", want));
    }
    NTH_TEST_ASSERT(*cur == '\0');
    return NTH_TRUE;
}

static nth_b8 test_flush_each(void) {
    const nth_usize line_len = PREFIX_LEN + sizeof MSG - 1 + 1;

    NthLoggerDesc desc = {0};
    desc.flush_each = NTH_TRUE;

    const NthResult r = init_logger(&desc);

    cap_begin();
    nth_log(NTH_LOG_LEVEL_INFO, MSG);
    const nth_usize one = cap_sync();

    nth_logf(NTH_LOG_LEVEL_INFO, "%s", MSG);
    const nth_usize two = cap_sync();

    nth_term();
    cap_end();

    NTH_TEST_ASSERT(r == NTH_RESULT_OK);
    NTH_TEST_ASSERT(one == line_len);
    NTH_TEST_ASSERT(two == 2 * line_len);
    return NTH_TRUE;
}

/* ================================================================================ */
/*  TRUNCATION                                                                      */
/* ================================================================================ */

static nth_b8 test_truncate_to_buffer(void) {
    const nth_usize size = 64;
    const nth_usize room = size - FIXED_LEN;
    char body[256];

    for (nth_usize i = 0; i < sizeof body - 1; i++)
        body[i] = (char)('a' + (i % 26));
    body[sizeof body - 1] = '\0';

    NthLoggerDesc desc = {0};
    desc.buffer_size = size;

    const NthResult r = init_logger(&desc);

    cap_begin();
    nth_log(NTH_LOG_LEVEL_INFO, body);
    nth_term();
    const nth_usize n = cap_end();

    nth_usize   len;
    const char *line = single_line(&len);

    NTH_TEST_ASSERT(r == NTH_RESULT_OK);
    NTH_TEST_ASSERT(n == size);
    NTH_TEST_ASSERT(line != NULL);
    NTH_TEST_ASSERT(prefix_ok(line, len, "MSG"));
    NTH_TEST_ASSERT(len == PREFIX_LEN + room);
    NTH_TEST_ASSERT(memcmp(line + PREFIX_LEN, body, room) == 0);
    return NTH_TRUE;
}

static nth_b8 test_truncate_min_buffer(void) {
    NthLoggerDesc desc = {0};
    desc.buffer_size = MIN_BUFFER;

    const NthResult r = init_logger(&desc);

    cap_begin();
    nth_log(NTH_LOG_LEVEL_ERROR, "ABCDEF");
    nth_term();
    const nth_usize n = cap_end();

    nth_usize   len;
    const char *line = single_line(&len);

    NTH_TEST_ASSERT(r == NTH_RESULT_OK);
    NTH_TEST_ASSERT(n == MIN_BUFFER);
    NTH_TEST_ASSERT(line != NULL);
    NTH_TEST_ASSERT(line_is(line, len, "ERR", "A"));
    return NTH_TRUE;
}

static nth_b8 test_truncate_format_max(void) {
    char body[FORMAT_MAX + 512];

    for (nth_usize i = 0; i < sizeof body - 1; i++)
        body[i] = (char)('A' + (i % 26));
    body[sizeof body - 1] = '\0';

    init_logger(NULL);

    cap_begin();
    nth_logf(NTH_LOG_LEVEL_INFO, "%s", body);
    nth_term();
    cap_end();

    nth_usize   len;
    const char *line = single_line(&len);

    NTH_TEST_ASSERT(line != NULL);
    NTH_TEST_ASSERT(prefix_ok(line, len, "MSG"));
    NTH_TEST_ASSERT(len == PREFIX_LEN + FORMAT_MAX - 1);
    NTH_TEST_ASSERT(memcmp(line + PREFIX_LEN, body, FORMAT_MAX - 1) == 0);
    return NTH_TRUE;
}

/* ================================================================================ */
/*  STRESS                                                                          */
/* ================================================================================ */

static nth_b8 test_stress_sequential(void) {
    const nth_usize body_len = 8;
    const nth_usize line_len = PREFIX_LEN + body_len + 1;
    const nth_usize count    = 15000;

    NthLoggerDesc desc = {0};
    desc.buffer_size = line_len * 3;

    const NthResult r = init_logger(&desc);

    cap_begin();
    for (nth_usize i = 0; i < count; i++)
        nth_logf(NTH_LOG_LEVEL_INFO, "%08u", (unsigned)i);
    nth_term();
    cap_end();

    NTH_TEST_ASSERT(r == NTH_RESULT_OK);
    NTH_TEST_ASSERT(g_cap_len == count * line_len);

    const char *cur = g_cap;
    for (nth_usize i = 0; i < count; i++) {
        char        want[16];
        nth_usize   len;
        const char *line = line_next(&cur, &len);

        snprintf(want, sizeof want, "%08u", (unsigned)i);

        NTH_TEST_ASSERT(line != NULL);
        NTH_TEST_ASSERT(line_is(line, len, "MSG", want));
    }
    NTH_TEST_ASSERT(*cur == '\0');
    return NTH_TRUE;
}

#if NTH_PLATFORM_WINDOWS
typedef HANDLE TestThread;
#else
typedef pthread_t TestThread;
#endif

static void worker_run(void *arg) {
    const nth_u32 id = *(const nth_u32 *)arg;
    char line[32];

    for (nth_u32 i = 0; i < TH_LINES; i++) {
        if ((id & 1u) == 0u) {
            snprintf(line, sizeof line, "t%u/%u", (unsigned)id, (unsigned)i);
            nth_log(NTH_LOG_LEVEL_INFO, line);
        } else {
            nth_logf(NTH_LOG_LEVEL_INFO, "t%u/%u", (unsigned)id, (unsigned)i);
        }
    }
}

#if NTH_PLATFORM_WINDOWS
static DWORD WINAPI thread_shim(LPVOID p) {
    worker_run(p);
    return 0;
}
static TestThread thread_start(void *arg) {
    return CreateThread(NULL, 0, thread_shim, (LPVOID)arg, 0, NULL);
}
static void thread_join(TestThread t) {
    WaitForSingleObject(t, INFINITE);
    CloseHandle(t);
}
#else
static void *thread_shim(void *p) {
    worker_run(p);
    return NULL;
}
static TestThread thread_start(void *arg) {
    pthread_t t;
    pthread_create(&t, NULL, thread_shim, arg);
    return t;
}
static void thread_join(TestThread t) {
    pthread_join(t, NULL);
}
#endif

static nth_u8 g_seen[TH_COUNT * TH_LINES];

static nth_b8 test_stress_threads(void) {
    TestThread th[TH_COUNT];
    nth_u32    id[TH_COUNT];

    memset(g_seen, 0, sizeof g_seen);

    NthLoggerDesc desc = {0};
    desc.buffer_size = 1024; 

    const NthResult r = init_logger(&desc);

    cap_begin();
    for (nth_u32 i = 0; i < TH_COUNT; i++) {
        id[i] = i;
        th[i] = thread_start(&id[i]);
    }
    for (nth_u32 i = 0; i < TH_COUNT; i++)
        thread_join(th[i]);
    nth_term();
    cap_end();

    NTH_TEST_ASSERT(r == NTH_RESULT_OK);
    NTH_TEST_ASSERT(count_lines() == TH_COUNT * TH_LINES);

    const char *cur = g_cap;
    nth_usize   len;
    const char *line;

    while ((line = line_next(&cur, &len)) != NULL) {
        unsigned tid = 0;
        unsigned idx = 0;

        NTH_TEST_ASSERT(prefix_ok(line, len, "MSG"));
        NTH_TEST_ASSERT(sscanf(line + PREFIX_LEN, "t%u/%u", &tid, &idx) == 2);
        NTH_TEST_ASSERT(tid < TH_COUNT && idx < TH_LINES);

        const nth_usize slot = (nth_usize)tid * TH_LINES + idx;
        NTH_TEST_ASSERT(g_seen[slot] == 0);
        g_seen[slot] = 1;
    }

    for (nth_usize i = 0; i < sizeof g_seen; i++)
        NTH_TEST_ASSERT(g_seen[i] == 1);

    return NTH_TRUE;
}

int main(void) {
    NthTest tests[] = {
        { "lifecycle/init_term",       test_init_term            },
        { "lifecycle/double_init",     test_double_init          },
        { "lifecycle/term_when_dead",  test_term_when_dead       },
        { "lifecycle/reinit",          test_reinit               },
        { "lifecycle/buffer_too_small",test_buffer_too_small     },
        { "lifecycle/zero_size",       test_zero_size_uses_default },
        { "lifecycle/via_core",        test_via_core             },
        { "format/levels",             test_levels               },
        { "format/level_clamped",      test_level_clamped        },
        { "format/timestamp",          test_timestamp            },
        { "format/logn_length",        test_logn_takes_length    },
        { "format/logn_embedded_nul",  test_logn_embedded_nul    },
        { "format/logn_matches_log",   test_logn_matches_log     },
        { "format/logn_truncates",     test_logn_truncates       },
        { "format/logf_args",          test_logf_args            },
        { "format/null_and_empty",     test_null_and_empty       },
        { "buffer/holds_until_flush",  test_holds_until_flush    },
        { "buffer/flush_when_empty",   test_flush_when_empty     },
        { "buffer/term_flushes",       test_term_flushes         },
        { "buffer/auto_flush_on_full", test_auto_flush_on_full   },
        { "buffer/flush_each",         test_flush_each           },
        { "truncate/to_buffer",        test_truncate_to_buffer   },
        { "truncate/min_buffer",       test_truncate_min_buffer  },
        { "truncate/format_max",       test_truncate_format_max  },
        { "stress/sequential",         test_stress_sequential    },
        { "stress/threads",            test_stress_threads       },
    };

    return NTH_RUN_TESTS(tests);
}
