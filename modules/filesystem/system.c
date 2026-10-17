#include <narthex/nth_fs.h>

#include <narthex/inl/lifecycle.h>
#include <narthex/utils/check.h>
#include <narthex/utils/compiler.h>
#include <narthex/utils/platform.h>

#include <stdlib.h>
#include <string.h>

#if NTH_PLATFORM_WINDOWS
#include <windows.h>
#else
#include <errno.h>
#include <unistd.h>
#endif

#define FS_DEFAULT_CAP ((nth_usize)4096)

#if NTH_PLATFORM_WINDOWS
#define FS_SEP '\\'
#define FS_IS_SEP(c) ((c) == '/' || (c) == '\\')
#else
#define FS_SEP '/'
#define FS_IS_SEP(c) ((c) == '/')
#endif


/* ================================================================================ */
/*  LAZY, THREAD-SAFE, COMPUTE-ONCE CACHE (cwd / exe / temp)                        */
/*                                                                                   */
/*  Reuses NthLifecycle's existing DEAD/STARTING/ALIVE one-shot contract rather     */
/*  than inventing a second lazy-init idiom. The contended window is a single OS    */
/*  call on first use ever — never again afterward — so losing the race just means  */
/*  a short spin, not a general purpose lock.                                      */
/* ================================================================================ */

typedef struct FsCachedPath {
    NthLifecycle life;
    char        *value; /* written only by the thread that wins begin_init; published via end_init's release-store */
} FsCachedPath;

static FsCachedPath g_fs_cwd;
static FsCachedPath g_fs_exe;
static FsCachedPath g_fs_temp;

static NthResult fs_cached_get(FsCachedPath *slot, NthResult (*compute)(char **), const char **out_value) {
    for(;;) {
        if(nth_lifecycle_is_alive(&slot->life)) {
            *out_value = slot->value;
            return NTH_RESULT_OK;
        }

        NthResult r = nth_lifecycle_begin_init(&slot->life);
        if(r == NTH_RESULT_OK) {
            char *value = NULL;
            NthResult cr = compute(&value);
            if(cr != NTH_RESULT_OK) {
                nth_lifecycle_fail_init(&slot->life); /* back to DEAD; a later call may succeed if conditions change */
                return cr;
            }
            slot->value = value;
            nth_lifecycle_end_init(&slot->life);
            *out_value = value;
            return NTH_RESULT_OK;
        }
        if(r == NTH_RESULT_ALREADY_INITIALIZED)
            continue; /* finished between our is_alive check and begin_init; next loop sees it */
        /* r == NTH_RESULT_NOT_READY: another thread is computing it right now — spin */
    }
}

static NthResult fs_compute_cwd(char **out) {
    nth_usize cap = FS_DEFAULT_CAP;
    char *buf = malloc(cap);
    if(NTH_UNLIKELY(buf == NULL)) return NTH_RESULT_OUT_OF_MEMORY;

#if NTH_PLATFORM_WINDOWS
    for(;;) {
        DWORD n = GetCurrentDirectoryA((DWORD)cap, buf);
        if(n == 0) { free(buf); return NTH_RESULT_PLATFORM_ERROR; }
        if(n < (DWORD)cap) break;
        cap = (nth_usize)n + 1;
        char *grown = realloc(buf, cap);
        if(NTH_UNLIKELY(grown == NULL)) { free(buf); return NTH_RESULT_OUT_OF_MEMORY; }
        buf = grown;
    }
#else
    while(getcwd(buf, cap) == NULL) {
        if(errno != ERANGE) { free(buf); return NTH_RESULT_PLATFORM_ERROR; }
        cap *= 2;
        char *grown = realloc(buf, cap);
        if(NTH_UNLIKELY(grown == NULL)) { free(buf); return NTH_RESULT_OUT_OF_MEMORY; }
        buf = grown;
    }
#endif

    *out = buf;
    return NTH_RESULT_OK;
}

static NthResult fs_compute_exe(char **out) {
    nth_usize cap = FS_DEFAULT_CAP;
    char *buf = malloc(cap);
    if(NTH_UNLIKELY(buf == NULL)) return NTH_RESULT_OUT_OF_MEMORY;

#if NTH_PLATFORM_WINDOWS
    for(;;) {
        DWORD n = GetModuleFileNameA(NULL, buf, (DWORD)cap);
        if(n == 0) { free(buf); return NTH_RESULT_PLATFORM_ERROR; }
        if(n < (DWORD)cap) break;
        cap *= 2;
        char *grown = realloc(buf, cap);
        if(NTH_UNLIKELY(grown == NULL)) { free(buf); return NTH_RESULT_OUT_OF_MEMORY; }
        buf = grown;
    }
#else
    for(;;) {
        ssize_t n = readlink("/proc/self/exe", buf, cap);
        if(n < 0) { free(buf); return NTH_RESULT_PLATFORM_ERROR; }
        if((nth_usize)n < cap) { buf[n] = '\0'; break; }
        cap *= 2;
        char *grown = realloc(buf, cap);
        if(NTH_UNLIKELY(grown == NULL)) { free(buf); return NTH_RESULT_OUT_OF_MEMORY; }
        buf = grown;
    }
#endif

    nth_usize len = strlen(buf); /* strip the filename, keep the containing directory */
    while(len > 0 && !FS_IS_SEP(buf[len - 1])) len--;
    if(len > 0) len--;
    buf[len] = '\0';

    *out = buf;
    return NTH_RESULT_OK;
}

static NthResult fs_compute_temp(char **out) {
#if NTH_PLATFORM_WINDOWS
    nth_usize cap = FS_DEFAULT_CAP;
    char *buf = malloc(cap);
    if(NTH_UNLIKELY(buf == NULL)) return NTH_RESULT_OUT_OF_MEMORY;

    for(;;) {
        DWORD n = GetTempPathA((DWORD)cap, buf);
        if(n == 0) { free(buf); return NTH_RESULT_PLATFORM_ERROR; }
        if(n < (DWORD)cap) break;
        cap = (nth_usize)n;
        char *grown = realloc(buf, cap);
        if(NTH_UNLIKELY(grown == NULL)) { free(buf); return NTH_RESULT_OUT_OF_MEMORY; }
        buf = grown;
    }

    nth_usize len = strlen(buf);
    if(len > 0 && FS_IS_SEP(buf[len - 1])) /* GetTempPathA includes a trailing separator; strip for consistency with cwd/exe */
        buf[len - 1] = '\0';
#else
    const char *env = getenv("TMPDIR");
    const char *src = (env != NULL && env[0] != '\0') ? env : "/tmp"; /* classic Unix fallback */

    nth_usize len = strlen(src);
    char *buf = malloc(len + 1);
    if(NTH_UNLIKELY(buf == NULL)) return NTH_RESULT_OUT_OF_MEMORY;
    memcpy(buf, src, len + 1);
#endif

    *out = buf;
    return NTH_RESULT_OK;
}

NthResult nth_fs_system_cwd(NthPath *out) {
    NTH_DASSERT(NTH_LIKELY(out != NULL));
    NTH_ASSERT(NTH_LIKELY(nth_path_is_alive(out)));

    const char *value;
    NthResult r = fs_cached_get(&g_fs_cwd, fs_compute_cwd, &value);
    if(r != NTH_RESULT_OK) return r;
    nth_path_set(out, value);
    return NTH_RESULT_OK;
}

NthResult nth_fs_system_exe(NthPath *out) {
    NTH_DASSERT(NTH_LIKELY(out != NULL));
    NTH_ASSERT(NTH_LIKELY(nth_path_is_alive(out)));

    const char *value;
    NthResult r = fs_cached_get(&g_fs_exe, fs_compute_exe, &value);
    if(r != NTH_RESULT_OK) return r;
    nth_path_set(out, value);
    return NTH_RESULT_OK;
}

NthResult nth_fs_system_temp(NthPath *out) {
    NTH_DASSERT(NTH_LIKELY(out != NULL));
    NTH_ASSERT(NTH_LIKELY(nth_path_is_alive(out)));

    const char *value;
    NthResult r = fs_cached_get(&g_fs_temp, fs_compute_temp, &value);
    if(r != NTH_RESULT_OK) return r;
    nth_path_set(out, value);
    return NTH_RESULT_OK;
}


/* ================================================================================ */
/*  CONFIG / DATA — computed fresh every call, app_name passed explicitly          */
/*  (cheap env lookups; caching would risk staleness if app_name ever varied)       */
/* ================================================================================ */

static char *fs_dup(const char *s) {
    nth_usize len = strlen(s);
    char *buf = malloc(len + 1);
    if(NTH_UNLIKELY(buf == NULL)) return NULL;
    memcpy(buf, s, len + 1);
    return buf;
}
static char *fs_dup_app(const char *base, const char *app_name) {
    if(app_name == NULL) return fs_dup(base);

    nth_usize base_len = strlen(base);
    nth_usize app_len = strlen(app_name);
    nth_usize len = base_len + 1 + app_len;

    char *buf = malloc(len + 1);
    if(NTH_UNLIKELY(buf == NULL)) return NULL;

    memcpy(buf, base, base_len);
    buf[base_len] = FS_SEP;
    memcpy(buf + base_len + 1, app_name, app_len);
    buf[len] = '\0';
    return buf;
}

NthResult nth_fs_system_config(const char *app_name, NthPath *out) {
    NTH_DASSERT(NTH_LIKELY(out != NULL));
    NTH_ASSERT(NTH_LIKELY(nth_path_is_alive(out)));

    char *base;
#if NTH_PLATFORM_WINDOWS
    const char *env = getenv("APPDATA");
    if(env == NULL) return NTH_RESULT_PLATFORM_ERROR;
    base = fs_dup(env);
#else
    const char *env = getenv("XDG_CONFIG_HOME");
    if(env != NULL && env[0] != '\0') {
        base = fs_dup(env);
    } else {
        const char *home = getenv("HOME");
        if(home == NULL) return NTH_RESULT_PLATFORM_ERROR;
        nth_usize len = strlen(home);
        base = malloc(len + 9);
        if(NTH_UNLIKELY(base == NULL)) return NTH_RESULT_OUT_OF_MEMORY;
        memcpy(base, home, len);
        memcpy(base + len, "/.config", 9);
    }
#endif
    if(NTH_UNLIKELY(base == NULL)) return NTH_RESULT_OUT_OF_MEMORY;

    char *final = fs_dup_app(base, app_name);
    free(base);
    if(NTH_UNLIKELY(final == NULL)) return NTH_RESULT_OUT_OF_MEMORY;

    nth_path_set(out, final);
    free(final);
    return NTH_RESULT_OK;
}

NthResult nth_fs_system_data(const char *app_name, NthPath *out) {
    NTH_DASSERT(NTH_LIKELY(out != NULL));
    NTH_ASSERT(NTH_LIKELY(nth_path_is_alive(out)));

    char *base;
#if NTH_PLATFORM_WINDOWS
    const char *env = getenv("LOCALAPPDATA");
    if(env == NULL) return NTH_RESULT_PLATFORM_ERROR;
    base = fs_dup(env);
#else
    const char *env = getenv("XDG_DATA_HOME");
    if(env != NULL && env[0] != '\0') {
        base = fs_dup(env);
    } else {
        const char *home = getenv("HOME");
        if(home == NULL) return NTH_RESULT_PLATFORM_ERROR;
        nth_usize len = strlen(home);
        base = malloc(len + 14);
        if(NTH_UNLIKELY(base == NULL)) return NTH_RESULT_OUT_OF_MEMORY;
        memcpy(base, home, len);
        memcpy(base + len, "/.local/share", 14);
    }
#endif
    if(NTH_UNLIKELY(base == NULL)) return NTH_RESULT_OUT_OF_MEMORY;

    char *final = fs_dup_app(base, app_name);
    free(base);
    if(NTH_UNLIKELY(final == NULL)) return NTH_RESULT_OUT_OF_MEMORY;

    nth_path_set(out, final);
    free(final);
    return NTH_RESULT_OK;
}