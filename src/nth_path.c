#include <narthex/nth_path.h>

#include <narthex/inl/lifecycle.h>
#include <narthex/utils/platform.h>
#include <narthex/utils/check.h>

#include "internal.h"

#include <stdlib.h>
#include <string.h>

#if NTH_PLATFORM_WINDOWS
#include <windows.h>
#else
#include <unistd.h>
#include <errno.h>
#endif


#define PATH_LEN_SHIFT   0
#define PATH_CAP_SHIFT   16
#define PATH_FLAGS_SHIFT 32
#define PATH_MAGIC_SHIFT 40
#define PATH_LEN_MASK   ((nth_u64)0xFFFF)
#define PATH_CAP_MASK   ((nth_u64)0xFFFF)
#define PATH_FLAGS_MASK ((nth_u64)0xFF)
#define PATH_MAGIC_MASK ((nth_u64)0xFFFFFF)

#define PATH_LEN_MAX ((nth_usize)PATH_LEN_MASK)
#define PATH_CAP_MAX ((nth_usize)PATH_CAP_MASK)

#define PATH_FLAG_OWNED      ((nth_u64)0x01) /* ptr came from malloc */
#define PATH_FLAG_INVALID    ((nth_u64)0x02) /* sticky, an allocation failed */
#define PATH_FLAG_NORMALIZED ((nth_u64)0x04) /* cleared by every mutation */

#define PATH_MAGIC ((nth_u64)0x4E5450) /* 'NTP' */


#define PATH_LEN(m)  ((nth_usize)(((m) >> PATH_LEN_SHIFT) & PATH_LEN_MASK))
#define PATH_CAP(m)  ((nth_usize)(((m) >> PATH_CAP_SHIFT) & PATH_CAP_MASK))
#define PATH_SET_LEN(m, v) ((m) = ((m) & ~(PATH_LEN_MASK << PATH_LEN_SHIFT)) | (((nth_u64)(v) & PATH_LEN_MASK) << PATH_LEN_SHIFT))
#define PATH_SET_CAP(m, v) ((m) = ((m) & ~(PATH_CAP_MASK << PATH_CAP_SHIFT)) | (((nth_u64)(v) & PATH_CAP_MASK) << PATH_CAP_SHIFT))

#define PATH_FLAGS(m) (((m) >> PATH_FLAGS_SHIFT) & PATH_FLAGS_MASK)

#define PATH_HAS(m, f)      ((((m) >> PATH_FLAGS_SHIFT) & (f)) != 0)
#define PATH_SET_FLAG(m, f) ((m) |= ((f) << PATH_FLAGS_SHIFT))
#define PATH_CLR_FLAG(m, f) ((m) &= ~((f) << PATH_FLAGS_SHIFT))

#define PATH_ALIVE(p) ((((p) >> PATH_MAGIC_SHIFT) & PATH_MAGIC_MASK) == PATH_MAGIC)
#define PATH_META_INIT (PATH_MAGIC << PATH_MAGIC_SHIFT)
#define PATH_META_DEAD ((nth_u64)0)


#if NTH_PLATFORM_WINDOWS
#define PATH_SEP '\\'
#define PATH_IS_SEP(c) ((c) == '/' || (c) == '\\')  // tolerate native separators in relative
#else
#define PATH_SEP '/'
#define PATH_IS_SEP(c) ((c) == '/')
#endif

#define PATH_DEFAULT_CAP ((nth_usize)4096)


static struct {
    const char *exe_dir;
    const char *cwd_dir;
    const char *config_dir;
    const char *data_dir;
} g_path;


static char *path_dup(const char *s) {
    nth_usize len = strlen(s);
    char *buf = malloc(len + 1);
    if(NTH_UNLIKELY(buf == NULL))
        return NULL;
    memcpy(buf, s, len + 1);
    return buf;
}
static char *path_dup_app(const char *base, const char *app_name) {
    nth_usize base_len = strlen(base);

    if(app_name == NULL)
        return path_dup(base);

    nth_usize app_len = strlen(app_name);
    nth_usize len = base_len + 1 + app_len;

    char *buf = malloc(len + 1);
    if(NTH_UNLIKELY(buf == NULL))
        return NULL;

    memcpy(buf, base, base_len);
    buf[base_len] = PATH_SEP;
    memcpy(buf + base_len + 1, app_name, app_len);
    buf[len] = '\0';

    return buf;
}
static NthResult path_grow(NthPath *path, nth_usize needed_len) {
    nth_usize cap = PATH_CAP(path->meta);

    if(NTH_LIKELY(needed_len < cap))
        return NTH_RESULT_OK;

    NTH_ASSERT(needed_len < PATH_LEN_MAX); /* 16-bit len/cap fields, hard ceiling */

    if(NTH_UNLIKELY(!PATH_HAS(path->meta, PATH_FLAG_OWNED))) {
        PATH_SET_FLAG(path->meta, PATH_FLAG_INVALID);
        return NTH_RESULT_OUT_OF_MEMORY;
    }

    nth_usize new_cap = cap + (cap >> 1); /* geometric growth */
    if(new_cap <= needed_len)
        new_cap = needed_len + 1;
    if(new_cap > PATH_CAP_MAX)
        new_cap = PATH_CAP_MAX;

    nth_u8 *grown = realloc(path->ptr, new_cap);
    if(NTH_UNLIKELY(grown == NULL)) {
        PATH_SET_FLAG(path->meta, PATH_FLAG_INVALID);
        return NTH_RESULT_OUT_OF_MEMORY;
    }

    path->ptr = grown;
    PATH_SET_CAP(path->meta, new_cap);
    return NTH_RESULT_OK;
}


/* ================================================================================ */
/*  IMPLEMENTATION                                                                  */
/* ================================================================================ */

NthResult nth_setup_path(NthPath *path) {
    NTH_DASSERT(NTH_LIKELY(path != NULL));
    NTH_CHECK(NTH_LIKELY(!PATH_ALIVE(path->meta)), NTH_RESULT_ALREADY_INITIALIZED);

    nth_u8 *tmp = malloc(PATH_DEFAULT_CAP);
    if(NTH_UNLIKELY(tmp == NULL))
        return NTH_RESULT_OUT_OF_MEMORY;

    tmp[0] = '\0';

    path->ptr = tmp;
    path->meta = PATH_META_INIT;
    PATH_SET_LEN(path->meta, 0);
    PATH_SET_CAP(path->meta, PATH_DEFAULT_CAP);
    PATH_SET_FLAG(path->meta, PATH_FLAG_OWNED);
    PATH_SET_FLAG(path->meta, PATH_FLAG_NORMALIZED);

    return NTH_RESULT_OK;
}
NthResult nth_setup_path_cstr(NthPath *path, const char *str) {
    NTH_DASSERT(NTH_LIKELY(path != NULL));
    NTH_CHECK(NTH_LIKELY(!PATH_ALIVE(path->meta)), NTH_RESULT_ALREADY_INITIALIZED);

    nth_usize len = strlen(str);
    NTH_ASSERT(len < PATH_LEN_MAX);

    nth_usize cap = (len + 1 > PATH_DEFAULT_CAP) ? (len + 1) : PATH_DEFAULT_CAP;

    nth_u8 *tmp = malloc(cap);
    if(NTH_UNLIKELY(tmp == NULL))
        return NTH_RESULT_OUT_OF_MEMORY;

    memcpy(tmp, str, len);
    tmp[len] = '\0';

    path->ptr = tmp;
    path->meta = PATH_META_INIT;
    PATH_SET_LEN(path->meta, len);
    PATH_SET_CAP(path->meta, cap);
    PATH_SET_FLAG(path->meta, PATH_FLAG_OWNED);

    return NTH_RESULT_OK;
}
NthResult nth_setup_path_view(NthPath *path, NthPathView view) {
    NTH_DASSERT(NTH_LIKELY(path != NULL));
    NTH_CHECK(NTH_LIKELY(!PATH_ALIVE(path->meta)), NTH_RESULT_ALREADY_INITIALIZED);

    NTH_ASSERT(view.size < PATH_LEN_MAX);
    nth_usize cap = (view.size + 1 > PATH_DEFAULT_CAP) ? (view.size + 1) : PATH_DEFAULT_CAP;

    nth_u8 *tmp = malloc(cap);
    if(NTH_UNLIKELY(tmp == NULL))
        return NTH_RESULT_OUT_OF_MEMORY;

    memcpy(tmp, view.base, view.size);
    tmp[view.size] = '\0';

    path->ptr = tmp;
    path->meta = PATH_META_INIT;
    PATH_SET_LEN(path->meta, view.size);
    PATH_SET_CAP(path->meta, cap);
    PATH_SET_FLAG(path->meta, PATH_FLAG_OWNED);

    return NTH_RESULT_OK;
}
void nth_teardown_path(NthPath *path) {
    NTH_DASSERT(NTH_LIKELY(path != NULL));
    NTH_CHECK(NTH_LIKELY(PATH_ALIVE(path->meta)), (void)0);

    if(PATH_HAS(path->meta, PATH_FLAG_OWNED) && path->ptr != NULL)
        free(path->ptr);

    path->ptr = NULL;
    path->meta = PATH_META_DEAD;
}


void nth_path_system_config(NthPath *path) { // XDG_CONFIG_HOME/<app> on Linux, %APPDATA%\<app> on Windows
    NTH_DASSERT(NTH_LIKELY(path != NULL));
    NTH_DASSERT(NTH_LIKELY(nth_lifecycle_is_alive(&g_narthex_life)));
    NTH_ASSERT(NTH_LIKELY(PATH_ALIVE(path->meta)));

    const char *dir = g_path.config_dir;
    if(NTH_UNLIKELY(dir == NULL)) {
        PATH_SET_FLAG(path->meta, PATH_FLAG_INVALID);
        return;
    }
    nth_path_set(path, dir);
}
void nth_path_system_data(NthPath *path) { // XDG_DATA_HOME/<app> on Linux, %LOCALAPPDATA%\<app> on Windows
    NTH_DASSERT(NTH_LIKELY(path != NULL));
    NTH_DASSERT(NTH_LIKELY(nth_lifecycle_is_alive(&g_narthex_life)));
    NTH_ASSERT(NTH_LIKELY(PATH_ALIVE(path->meta)));

    const char *dir = g_path.data_dir;
    if(NTH_UNLIKELY(dir == NULL)) {
        PATH_SET_FLAG(path->meta, PATH_FLAG_INVALID);
        return;
    }
    nth_path_set(path, dir);
}
void nth_path_system_cwd(NthPath *path) { // working directory at init time
    NTH_DASSERT(NTH_LIKELY(path != NULL));
    NTH_DASSERT(NTH_LIKELY(nth_lifecycle_is_alive(&g_narthex_life)));
    NTH_ASSERT(NTH_LIKELY(PATH_ALIVE(path->meta)));

    const char *cwd = g_path.cwd_dir;
    if(NTH_UNLIKELY(cwd == NULL)) {
        PATH_SET_FLAG(path->meta, PATH_FLAG_INVALID);
        return;
    }
    nth_path_set(path, cwd);
}
void nth_path_system_exe(NthPath *path) { // directory of running executable
    NTH_DASSERT(NTH_LIKELY(path != NULL));
    NTH_DASSERT(NTH_LIKELY(nth_lifecycle_is_alive(&g_narthex_life)));
    NTH_ASSERT(NTH_LIKELY(PATH_ALIVE(path->meta)));

    const char *dir = g_path.exe_dir;
    if(NTH_UNLIKELY(dir == NULL)) {
        PATH_SET_FLAG(path->meta, PATH_FLAG_INVALID);
        return;
    }
    nth_path_set(path, dir);
}


void nth_path_set(NthPath *path, const char *str) {
    NTH_DASSERT(NTH_LIKELY(path != NULL));
    NTH_DASSERT(NTH_LIKELY(str != NULL));
    NTH_ASSERT(NTH_LIKELY(PATH_ALIVE(path->meta)));

    nth_path_setn(path, str, strlen(str));
}
void nth_path_setn(NthPath *path, const char *str, nth_usize len) {
    NTH_DASSERT(NTH_LIKELY(path != NULL));
    NTH_DASSERT(NTH_LIKELY(str != NULL));
    NTH_ASSERT(NTH_LIKELY(PATH_ALIVE(path->meta)));
    // NTH_CHECK(len != 0, (void)0);

    if(NTH_UNLIKELY(path_grow(path, len) != NTH_RESULT_OK))
        return;

    memcpy(path->ptr, str, len);
    path->ptr[len] = '\0';
    PATH_SET_LEN(path->meta, len);
    PATH_CLR_FLAG(path->meta, PATH_FLAG_NORMALIZED);
}

void nth_path_append(NthPath *path, const char *str) {
    NTH_DASSERT(NTH_LIKELY(path != NULL));
    NTH_ASSERT(NTH_LIKELY(PATH_ALIVE(path->meta)));

    nth_path_appendn(path, str, strlen(str));
}
void nth_path_appendn(NthPath *path, const char *str, nth_usize len) {
    NTH_DASSERT(NTH_LIKELY(path != NULL));
    NTH_DASSERT(NTH_LIKELY(str != NULL));
    NTH_ASSERT(NTH_LIKELY(PATH_ALIVE(path->meta)));
    
    NTH_CHECK(len != 0, (void)0);

    nth_usize cur = PATH_LEN(path->meta);
    nth_b8 need_sep = cur > 0 && !PATH_IS_SEP(path->ptr[cur-1]);
    nth_usize new_len = cur + (need_sep ? 1 : 0) + len;

    if(NTH_UNLIKELY(path_grow(path, new_len) != NTH_RESULT_OK))
        return;

    if(need_sep)
        path->ptr[cur++] = PATH_SEP;
    memcpy(&path->ptr[cur], str, len);
    path->ptr[cur + len] = '\0';

    PATH_SET_LEN(path->meta, new_len);
    PATH_CLR_FLAG(path->meta, PATH_FLAG_NORMALIZED);
}

void nth_path_normalize(NthPath *path) {
    NTH_DASSERT(NTH_LIKELY(path != NULL));
    NTH_ASSERT(NTH_LIKELY(PATH_ALIVE(path->meta)));

    if(PATH_HAS(path->meta, PATH_FLAG_NORMALIZED))
        return;

    nth_usize len = PATH_LEN(path->meta);
    nth_u8 *buf = path->ptr;
    nth_usize read = 0;
    nth_usize write = 0;
    nth_b8 absolute = 0;

    if(len > 0 && PATH_IS_SEP(buf[0])) {
        absolute = 1;
        buf[write++] = PATH_SEP;
        read = 1;
        while(read < len && PATH_IS_SEP(buf[read])) read++;
    }
#if NTH_PLATFORM_WINDOWS
    else if(len >= 2 && buf[1] == ':' &&
            ((buf[0] >= 'A' && buf[0] <= 'Z') || (buf[0] >= 'a' && buf[0] <= 'z'))) {
        absolute = 1;
        buf[write++] = buf[0];
        buf[write++] = ':';
        read = 2;
        if(read < len && PATH_IS_SEP(buf[read])) {
            buf[write++] = PATH_SEP;
            while(read < len && PATH_IS_SEP(buf[read])) read++;
        }
    }
#endif

    nth_usize root_end = write;

    /* stack holds, per real segment, the write-offset to rewind to if a later ".." cancels it */
    nth_usize stack_cap = 16;
    nth_usize stack_len = 0;
    nth_usize *stack = malloc(stack_cap * sizeof(nth_usize));
    if(NTH_UNLIKELY(stack == NULL)) {
        PATH_SET_FLAG(path->meta, PATH_FLAG_INVALID);
        return;
    }

    while(read < len) {
        nth_usize seg_start = read;
        while(read < len && !PATH_IS_SEP(buf[read])) read++;
        nth_usize seg_len = read - seg_start;

        nth_usize next = read;
        while(next < len && PATH_IS_SEP(buf[next])) next++;

        if(seg_len == 0) {
            /* nothing */
        } else if(seg_len == 1 && buf[seg_start] == '.') {
            /* nothing */
        } else if(seg_len == 2 && buf[seg_start] == '.' && buf[seg_start + 1] == '.') {
            if(stack_len > 0) {
                write = stack[--stack_len];
            } else if(!absolute) {
                if(write > root_end) buf[write++] = PATH_SEP;
                buf[write++] = '.';
                buf[write++] = '.';
            }
        } else {
            nth_usize rewind_to = write;
            if(write > root_end) buf[write++] = PATH_SEP;

            if(NTH_UNLIKELY(stack_len == stack_cap)) {
                nth_usize new_cap = stack_cap * 2;
                nth_usize *grown = realloc(stack, new_cap * sizeof(nth_usize));
                if(NTH_UNLIKELY(grown == NULL)) {
                    free(stack);
                    PATH_SET_FLAG(path->meta, PATH_FLAG_INVALID);
                    return;
                }
                stack = grown;
                stack_cap = new_cap;
            }
            stack[stack_len++] = rewind_to;

            memmove(&buf[write], &buf[seg_start], seg_len);
            write += seg_len;
        }

        read = next;
    }

    free(stack);

    if(write == 0)
        buf[write++] = '.';

    buf[write] = '\0';
    PATH_SET_LEN(path->meta, write);
    PATH_SET_FLAG(path->meta, PATH_FLAG_NORMALIZED);
}
void nth_path_clear(NthPath *path) {
    NTH_DASSERT(NTH_LIKELY(path != NULL));
    NTH_ASSERT(NTH_LIKELY(PATH_ALIVE(path->meta)));

    path->ptr[0] = '\0';
    PATH_SET_LEN(path->meta, 0);
    PATH_SET_FLAG(path->meta, PATH_FLAG_NORMALIZED);
    PATH_CLR_FLAG(path->meta, PATH_FLAG_INVALID);
}
void nth_path_copy(NthPath *dst, const NthPath *src) {
    NTH_DASSERT(NTH_LIKELY(dst != NULL));
    NTH_DASSERT(NTH_LIKELY(src != NULL));
    NTH_ASSERT(NTH_LIKELY(PATH_ALIVE(dst->meta)));
    NTH_ASSERT(NTH_LIKELY(PATH_ALIVE(src->meta)));

    nth_usize len = PATH_LEN(src->meta);

    if(NTH_UNLIKELY(path_grow(dst, len) != NTH_RESULT_OK))
        return;

    memcpy(dst->ptr, src->ptr, len);
    dst->ptr[len] = '\0';
    PATH_SET_LEN(dst->meta, len);

    if(PATH_HAS(src->meta, PATH_FLAG_NORMALIZED))
        PATH_SET_FLAG(dst->meta, PATH_FLAG_NORMALIZED);
    else
        PATH_CLR_FLAG(dst->meta, PATH_FLAG_NORMALIZED);
}
// void nth_path_make_absolute(NthPath *path);
// void nth_path_make_relative(NthPath *path);
// void nth_path_set_extension(NthPath *path, const char *extension);
// void nth_path_remove_extension(NthPath *path);

const char *nth_path_data(const NthPath *path) {
    NTH_DASSERT(NTH_LIKELY(path != NULL));
    NTH_ASSERT(NTH_LIKELY(PATH_ALIVE(path->meta)));
    return (const char *)path->ptr;
}
nth_usize nth_path_length(const NthPath *path) {
    NTH_DASSERT(NTH_LIKELY(path != NULL));
    NTH_ASSERT(NTH_LIKELY(PATH_ALIVE(path->meta)));
    return PATH_LEN(path->meta);
}
nth_b8 nth_path_empty(const NthPath *path) {
    NTH_DASSERT(NTH_LIKELY(path != NULL));
    NTH_ASSERT(NTH_LIKELY(PATH_ALIVE(path->meta)));
    return PATH_LEN(path->meta) == 0;
}
nth_b8 nth_path_is_absolute(const NthPath *path) {
    NTH_DASSERT(NTH_LIKELY(path != NULL));
    NTH_ASSERT(NTH_LIKELY(PATH_ALIVE(path->meta)));

    nth_usize len = PATH_LEN(path->meta);
    if(len == 0)
        return NTH_FALSE;

#if NTH_PLATFORM_WINDOWS
    if(len >= 3 &&
       ((path->ptr[0] >= 'A' && path->ptr[0] <= 'Z') || (path->ptr[0] >= 'a' && path->ptr[0] <= 'z')) &&
       path->ptr[1] == ':' && PATH_IS_SEP(path->ptr[2]))
        return NTH_TRUE;
    if(PATH_IS_SEP(path->ptr[0]))
        return NTH_TRUE;
    return NTH_FALSE;
#else
    return PATH_IS_SEP(path->ptr[0]) ? NTH_TRUE : NTH_FALSE;
#endif
}
nth_b8 nth_path_is_relative(const NthPath *path) {
    NTH_DASSERT(NTH_LIKELY(path != NULL));
    NTH_ASSERT(NTH_LIKELY(PATH_ALIVE(path->meta)));
    return !nth_path_is_absolute(path);
}

NthPathView nth_path_filename(const NthPath *path) {
    NTH_DASSERT(NTH_LIKELY(path != NULL));
    NTH_ASSERT(NTH_LIKELY(PATH_ALIVE(path->meta)));

    NthPathView view = {0};

    nth_usize len = PATH_LEN(path->meta);
    if(len == 0 || PATH_IS_SEP(path->ptr[len - 1]))
        return view;

    nth_usize start = len;
    while(start > 0 && !PATH_IS_SEP(path->ptr[start - 1])) start--;

    view.base = &path->ptr[start];
    view.size = len - start;
    return view;
}
NthPathView nth_path_directory(const NthPath *path) {
    NTH_DASSERT(NTH_LIKELY(path != NULL));
    NTH_ASSERT(NTH_LIKELY(PATH_ALIVE(path->meta)));

    NthPathView view = {0};

    nth_usize len = PATH_LEN(path->meta);
    nth_usize end = len;
    while(end > 0 && PATH_IS_SEP(path->ptr[end - 1])) end--;

    if(end == 0) {
        view.base = path->ptr;
        view.size = (len > 0) ? 1 : 0; /* "/", "///", "" -> canonical root or empty */
        return view;
    }

    nth_usize start = end;
    while(start > 0 && !PATH_IS_SEP(path->ptr[start - 1])) start--;

    if(start == 0) {
        view.base = path->ptr;
        view.size = 0; /* no separator anywhere -> no directory part */
        return view;
    }

    if(end == len) {
        /* no trailing separator: split off the filename */
        nth_usize dir_end = start;
        while(dir_end > 1 && PATH_IS_SEP(path->ptr[dir_end - 1]))
            dir_end--;
        view.base = path->ptr;
        view.size = dir_end;
    } else {
        /* trailing separator(s): no filename, whole trimmed path is the directory */
        view.base = path->ptr;
        view.size = end;
    }

    return view;
}
NthPathView nth_path_extension(const NthPath *path) {
    NTH_DASSERT(NTH_LIKELY(path != NULL));
    NTH_ASSERT(NTH_LIKELY(PATH_ALIVE(path->meta)));

    NthPathView fname = nth_path_filename(path);
    NthPathView view = {0};
    if(fname.base == NULL || fname.size == 0)
        return view;

    nth_usize i = fname.size;
    while(i > 1) {
        i--;
        if(fname.base[i] == '.') {
            nth_usize j = 0;
            while(j < i && fname.base[j] == '.') j++;
            if(j == i)
                return view; /* filename is all dots (".", "..", "...") -> no extension */

            view.base = &fname.base[i];
            view.size = fname.size - i;
            return view;
        }
    }
    return view;
}
NthPathView nth_path_stem(const NthPath *path) {
    NTH_DASSERT(NTH_LIKELY(path != NULL));
    NTH_ASSERT(NTH_LIKELY(PATH_ALIVE(path->meta)));

    NthPathView fname = nth_path_filename(path);
    NthPathView ext = nth_path_extension(path);

    if(ext.base != NULL)
        fname.size -= ext.size;
    return fname;
}

nth_b8 nth_path_equal(const NthPath *a, const NthPath *b) {
    NTH_DASSERT(NTH_LIKELY(a != NULL));
    NTH_DASSERT(NTH_LIKELY(b != NULL));
    NTH_ASSERT(NTH_LIKELY(PATH_ALIVE(a->meta)));
    NTH_ASSERT(NTH_LIKELY(PATH_ALIVE(b->meta)));

    NthPathView bv;
    bv.base = b->ptr;
    bv.size = PATH_LEN(b->meta);
    return nth_path_equal_view(a, bv);
}
nth_b8 nth_path_equal_view(const NthPath *path, NthPathView view) {
    NTH_DASSERT(NTH_LIKELY(path != NULL));
    NTH_ASSERT(NTH_LIKELY(PATH_ALIVE(path->meta)));

    NthPathView self;
    self.base = path->ptr;
    self.size = PATH_LEN(path->meta);
    return nth_path_view_equal(self, view);
}
nth_b8 nth_path_view_equal(NthPathView a, NthPathView b) {
    if(a.size != b.size)
        return 0;
    if(a.size == 0)
        return 1;
    return memcmp(a.base, b.base, a.size) == 0;
}
nth_b8 nth_path_view_equal_cstr(NthPathView view, const char *str) {
    nth_usize len = strlen(str);
    if(view.size != len)
        return 0;
    if(len == 0)
        return 1;
    return memcmp(view.base, str, len) == 0;
}


/* ================================================================================ */
/*  LIFE-CYCLE                                                                      */
/* ================================================================================ */

NthResult nth_init_path(const char *app_name) {
    NthResult r = NTH_RESULT_OK;

    { // CWD
        nth_usize cap = PATH_DEFAULT_CAP;
        char *buf = malloc(cap);

        if(NTH_UNLIKELY(buf == NULL)) {
            r = NTH_RESULT_OUT_OF_MEMORY;
            goto fail;
        }

#if NTH_PLATFORM_WINDOWS
        for(;;) {
            DWORD n = GetCurrentDirectoryA((DWORD)cap, buf);
            if(n == 0) {
                free(buf);
                r = NTH_RESULT_PLATFORM_ERROR;
                goto fail;
            }
            if(n < (DWORD)cap)
                break;
            cap = (nth_usize)n + 1;
            char *grown = realloc(buf, cap);
            if(NTH_UNLIKELY(grown == NULL)) {
                free(buf);
                r = NTH_RESULT_OUT_OF_MEMORY;
                goto fail;
            }
            buf = grown;
        }
#else
        while(getcwd(buf, cap) == NULL) {
            if(errno != ERANGE) {
                free(buf);
                r = NTH_RESULT_PLATFORM_ERROR;
                goto fail;
            }
            cap *= 2;
            char *grown = realloc(buf, cap);
            if(NTH_UNLIKELY(grown == NULL)) {
                free(buf);
                r = NTH_RESULT_OUT_OF_MEMORY;
                goto fail;
            }
            buf = grown;
        }
#endif

        g_path.cwd_dir = buf;
    }
    { // EXE
        nth_usize cap = PATH_DEFAULT_CAP;
        char *buf = malloc(cap);

        if(NTH_UNLIKELY(buf == NULL)) {
            r = NTH_RESULT_OUT_OF_MEMORY;
            goto fail;
        }

#if NTH_PLATFORM_WINDOWS
        for(;;) {
            DWORD n = GetModuleFileNameA(NULL, buf, (DWORD)cap);
            if(n == 0) {
                free(buf);
                r = NTH_RESULT_PLATFORM_ERROR;
                goto fail;
            }
            if(n < (DWORD)cap)
                break;
            cap *= 2;
            char *grown = realloc(buf, cap);
            if(NTH_UNLIKELY(grown == NULL)) {
                free(buf);
                r = NTH_RESULT_OUT_OF_MEMORY;
                goto fail;
            }
            buf = grown;
        }
#else
        for(;;) {
            ssize_t n = readlink("/proc/self/exe", buf, cap);
            if(n < 0) {
                free(buf);
                r = NTH_RESULT_PLATFORM_ERROR;
                goto fail;
            }
            if((nth_usize)n < cap) {
                buf[n] = '\0';
                break;
            }
            cap *= 2;
            char *grown = realloc(buf, cap);
            if(NTH_UNLIKELY(grown == NULL)) {
                free(buf);
                r = NTH_RESULT_OUT_OF_MEMORY;
                goto fail;
            }
            buf = grown;
        }
#endif

        nth_usize len = strlen(buf);

        while(len > 0 && !PATH_IS_SEP(buf[len - 1]))
            len--;

        if(len > 0)
            len--;

        buf[len] = '\0';

        g_path.exe_dir = buf;
    }
    { // Config
        char *base = NULL;

#if NTH_PLATFORM_WINDOWS
        const char *env = getenv("APPDATA");
        if(env == NULL) {
            r = NTH_RESULT_PLATFORM_ERROR;
            goto fail;
        }
        base = path_dup(env);
#else
        const char *env = getenv("XDG_CONFIG_HOME");
        if(env != NULL && env[0] != '\0') {
            base = path_dup(env);
        } else {
            const char *home = getenv("HOME");
            if(home == NULL) {
                r = NTH_RESULT_PLATFORM_ERROR;
                goto fail;
            }
            nth_usize len = strlen(home);
            base = malloc(len + 9);
            if(NTH_UNLIKELY(base == NULL)) {
                r = NTH_RESULT_OUT_OF_MEMORY;
                goto fail;
            }
            memcpy(base, home, len);
            memcpy(base + len, "/.config", 9);
        }
#endif

        if(NTH_UNLIKELY(base == NULL)) {
            r = NTH_RESULT_OUT_OF_MEMORY;
            goto fail;
        }

        if(app_name != NULL) {
            char *app_dir = path_dup_app(base, app_name);

            if(NTH_UNLIKELY(app_dir == NULL)) {
                free(base);
                r = NTH_RESULT_OUT_OF_MEMORY;
                goto fail;
            }

            free(base);
            base = app_dir;
        }

        g_path.config_dir = base;
    }
    { // Data
        char *base = NULL;

#if NTH_PLATFORM_WINDOWS
        const char *env = getenv("LOCALAPPDATA");
        if(env == NULL) {
            r = NTH_RESULT_PLATFORM_ERROR;
            goto fail;
        }
        base = path_dup(env);
#else
        const char *env = getenv("XDG_DATA_HOME");
        if(env != NULL && env[0] != '\0') {
            base = path_dup(env);
        } else {
            const char *home = getenv("HOME");
            if(home == NULL) {
                r = NTH_RESULT_PLATFORM_ERROR;
                goto fail;
            }
            nth_usize len = strlen(home);
            base = malloc(len + 14);
            if(NTH_UNLIKELY(base == NULL)) {
                r = NTH_RESULT_OUT_OF_MEMORY;
                goto fail;
            }
            memcpy(base, home, len);
            memcpy(base + len, "/.local/share", 14);
        }
#endif

        if(NTH_UNLIKELY(base == NULL)) {
            r = NTH_RESULT_OUT_OF_MEMORY;
            goto fail;
        }

        if(app_name != NULL) {
            char *app_dir = path_dup_app(base, app_name);

            if(NTH_UNLIKELY(app_dir == NULL)) {
                free(base);
                r = NTH_RESULT_OUT_OF_MEMORY;
                goto fail;
            }

            free(base);
            base = app_dir;
        }

        g_path.data_dir = base;
    }

    return NTH_RESULT_OK;

fail:
    if(g_path.exe_dir != NULL)
        free((void *)g_path.exe_dir);

    if(g_path.cwd_dir != NULL)
        free((void *)g_path.cwd_dir);

    if(g_path.config_dir != NULL)
        free((void *)g_path.config_dir);

    if(g_path.data_dir != NULL)
        free((void *)g_path.data_dir);

    g_path.exe_dir = NULL;
    g_path.cwd_dir = NULL;
    g_path.config_dir = NULL;
    g_path.data_dir = NULL;

    return r;
}

void nth_term_path(void) {
    if(g_path.exe_dir != NULL)
        free((void *)g_path.exe_dir);

    if(g_path.cwd_dir != NULL)
        free((void *)g_path.cwd_dir);

    if(g_path.config_dir != NULL)
        free((void *)g_path.config_dir);

    if(g_path.data_dir != NULL)
        free((void *)g_path.data_dir);

    g_path.exe_dir = NULL;
    g_path.cwd_dir = NULL;
    g_path.config_dir = NULL;
    g_path.data_dir = NULL;
}