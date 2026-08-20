#include "nth_test.h"

#include <narthex/narthex.h>
#include <narthex/nth_path.h>
#include <narthex/utils/platform.h>

#include <string.h>


/* ================================================================================ */
/*  POLICY KNOBS                                                                    */
/* ================================================================================ */

/* The header does not pin these two choices, so the suite encodes them here rather
   than scattering the assumption across the tests. Flip a knob and the expectations
   follow; every other test holds under either setting. */

/* 1: nth_path_append inserts a separator between two non-empty parts ("a" + "b" ->
      "a/b"). 0: it concatenates raw ("a" + "b" -> "ab"). */
#ifndef PATH_APPEND_JOINS
    #define PATH_APPEND_JOINS 1
#endif

/* 1: nth_path_extension returns ".txt". 0: it returns "txt". */
#ifndef PATH_EXT_INCLUDES_DOT
    #define PATH_EXT_INCLUDES_DOT 1
#endif


/* ================================================================================ */
/*  PLATFORM                                                                        */
/* ================================================================================ */

#if NTH_PLATFORM_WINDOWS
    #define SEP  "\\"
    #define SEPC '\\'

    #define ABS_DEEP "C:\\Windows\\System32"
    #define ABS_ROOT "C:\\"
    #define REL_DEEP "Windows\\System32"
#else
    #define SEP  "/"
    #define SEPC '/'

    #define ABS_DEEP "/usr/local/lib"
    #define ABS_ROOT "/"
    #define REL_DEEP "usr/local/lib"
#endif

#define LONG_PATH_PARTS 400
#define STRESS_IT       20000


/* ================================================================================ */
/*  HELPERS                                                                         */
/* ================================================================================ */

static NthPathView view_make(char *s, nth_usize n) {
    NthPathView v;
    v.base = (nth_u8 *)s;
    v.size = n;
    return v;
}

static nth_b8 path_is(const NthPath *p, const char *s) {
    return (nth_b8)(strcmp(nth_path_data(p), s) == 0);
}

static nth_b8 view_is(NthPathView v, const char *s) {
    const nth_usize n = strlen(s);

    if (v.size != n)
        return NTH_FALSE;
    if (n == 0)
        return NTH_TRUE;

    return (nth_b8)(memcmp(v.base, s, n) == 0);
}

/* A view must address bytes of the path it came from. */
static nth_b8 view_within(NthPathView v, const NthPath *p) {
    const nth_u8   *base = (const nth_u8 *)nth_path_data(p);
    const nth_usize len  = nth_path_length(p);

    if (v.size == 0)
        return NTH_TRUE; /* base is unconstrained for an empty view */

    NTH_TEST_ASSERT(v.size <= len);
    NTH_TEST_ASSERT(v.base >= base);
    NTH_TEST_ASSERT((nth_usize)(v.base - base) <= len - v.size);

    return NTH_TRUE;
}

/* Holds after every operation, on every platform, under every policy choice. */
static nth_b8 inv(const NthPath *p) {
    const char *d = nth_path_data(p);
    NTH_TEST_ASSERT(d != NULL);

    const nth_usize len = nth_path_length(p);

    /* data is a C string of exactly length bytes: no missing NUL, no embedded one */
    NTH_TEST_ASSERT(strlen(d) == len);

    NTH_TEST_ASSERT((nth_path_empty(p) != 0) == (len == 0));
    NTH_TEST_ASSERT(!(nth_path_is_absolute(p) && nth_path_is_relative(p)));
    if (len != 0)
        NTH_TEST_ASSERT(nth_path_is_absolute(p) || nth_path_is_relative(p));

    NTH_TEST_ASSERT(nth_path_equal(p, p));

    NTH_TEST_ASSERT(view_within(nth_path_filename(p), p));
    NTH_TEST_ASSERT(view_within(nth_path_directory(p), p));
    NTH_TEST_ASSERT(view_within(nth_path_extension(p), p));
    NTH_TEST_ASSERT(view_within(nth_path_stem(p), p));

    return NTH_TRUE;
}

/* stem and extension partition the filename, whichever side owns the dot. */
static nth_b8 check_split(const NthPath *p) {
    const NthPathView fn = nth_path_filename(p);
    const NthPathView st = nth_path_stem(p);
    const NthPathView ex = nth_path_extension(p);

#if PATH_EXT_INCLUDES_DOT
    const nth_usize dot = 0;
#else
    const nth_usize dot = (ex.size != 0) ? 1 : 0;
#endif

    NTH_TEST_ASSERT(st.size + dot + ex.size == fn.size);

    if (st.size != 0)
        NTH_TEST_ASSERT(memcmp(st.base, fn.base, st.size) == 0);
    if (ex.size != 0)
        NTH_TEST_ASSERT(memcmp(ex.base, fn.base + st.size + dot, ex.size) == 0);

    return NTH_TRUE;
}

/* directory is a prefix, filename is a suffix, at most one separator between them.
   Only meaningful for paths with no repeated separators. */
static nth_b8 check_dir_filename(const NthPath *p) {
    const NthPathView dir = nth_path_directory(p);
    const NthPathView fn  = nth_path_filename(p);

    const nth_u8   *base = (const nth_u8 *)nth_path_data(p);
    const nth_usize len  = nth_path_length(p);

    NTH_TEST_ASSERT(dir.size + fn.size <= len);
    NTH_TEST_ASSERT(len - dir.size - fn.size <= 1);

    if (dir.size != 0)
        NTH_TEST_ASSERT(dir.base == base);
    if (fn.size != 0)
        NTH_TEST_ASSERT(fn.base + fn.size == base + len);

    return NTH_TRUE;
}

static unsigned g_rs = 2463534242u;
static unsigned rnd(void) {
    g_rs = g_rs * 1664525u + 1013904223u;
    return g_rs >> 8;
}


/* ================================================================================ */
/*  SETUP / TEARDOWN                                                                */
/* ================================================================================ */

static nth_b8 test_setup_empty(void) {
    NthPath p;
    NTH_TEST_ASSERT(nth_setup_path(&p) == NTH_RESULT_OK);

    NTH_TEST_ASSERT(inv(&p));
    NTH_TEST_ASSERT(nth_path_empty(&p));
    NTH_TEST_ASSERT(nth_path_length(&p) == 0);
    NTH_TEST_ASSERT(path_is(&p, ""));
    NTH_TEST_ASSERT(!nth_path_is_absolute(&p));

    nth_teardown_path(&p);
    return NTH_TRUE;
}

static nth_b8 test_setup_str(void) {
    NthPath p;
    NTH_TEST_ASSERT(nth_setup_path_cstr(&p, "alpha" SEP "beta") == NTH_RESULT_OK);

    NTH_TEST_ASSERT(inv(&p));
    NTH_TEST_ASSERT(path_is(&p, "alpha" SEP "beta"));
    NTH_TEST_ASSERT(nth_path_length(&p) == strlen("alpha" SEP "beta"));
    NTH_TEST_ASSERT(!nth_path_empty(&p));

    nth_teardown_path(&p);
    return NTH_TRUE;
}

static nth_b8 test_setup_str_empty(void) {
    NthPath p;
    NTH_TEST_ASSERT(nth_setup_path_cstr(&p, "") == NTH_RESULT_OK);

    NTH_TEST_ASSERT(inv(&p));
    NTH_TEST_ASSERT(nth_path_empty(&p));
    NTH_TEST_ASSERT(path_is(&p, ""));

    nth_teardown_path(&p);
    return NTH_TRUE;
}

/* The source is sized, not terminated. An implementation that reaches for strlen
   walks off the end of the array. */
static nth_b8 test_setup_view_unterminated(void) {
    char raw[4] = { 'a', 'b', 'c', 'd' };

    NthPath p;
    NTH_TEST_ASSERT(nth_setup_path_view(&p, view_make(raw, 3)) == NTH_RESULT_OK);

    NTH_TEST_ASSERT(inv(&p));
    NTH_TEST_ASSERT(nth_path_length(&p) == 3);
    NTH_TEST_ASSERT(path_is(&p, "abc"));

    /* the copy is independent of the source */
    raw[0] = 'z';
    NTH_TEST_ASSERT(path_is(&p, "abc"));

    nth_teardown_path(&p);
    return NTH_TRUE;
}

static nth_b8 test_setup_view_empty(void) {
    char raw[] = "ignored";

    NthPath a, b;
    NTH_TEST_ASSERT(nth_setup_path_view(&a, view_make(raw, 0)) == NTH_RESULT_OK);
    NTH_TEST_ASSERT(nth_setup_path_view(&b, view_make(NULL, 0)) == NTH_RESULT_OK);

    NTH_TEST_ASSERT(inv(&a));
    NTH_TEST_ASSERT(inv(&b));
    NTH_TEST_ASSERT(nth_path_empty(&a));
    NTH_TEST_ASSERT(nth_path_empty(&b));
    NTH_TEST_ASSERT(nth_path_equal(&a, &b));

    nth_teardown_path(&a);
    nth_teardown_path(&b);
    return NTH_TRUE;
}

/* A torn-down path must be reusable through setup, and the second teardown must
   not double free. Run under ASan to make this test mean anything. */
static nth_b8 test_teardown_then_reuse(void) {
    NthPath p;

    for (nth_usize i = 0; i < 8; i++) {
        NTH_TEST_ASSERT(nth_setup_path_cstr(&p, ABS_DEEP) == NTH_RESULT_OK);
        NTH_TEST_ASSERT(inv(&p));

        nth_path_append(&p, "extra");
        NTH_TEST_ASSERT(inv(&p));

        nth_teardown_path(&p);
    }

    NTH_TEST_ASSERT(nth_setup_path(&p) == NTH_RESULT_OK);
    NTH_TEST_ASSERT(nth_path_empty(&p));
    nth_teardown_path(&p);

    return NTH_TRUE;
}


/* ================================================================================ */
/*  SET                                                                             */
/* ================================================================================ */

static nth_b8 test_set_replaces(void) {
    NthPath p;
    NTH_TEST_ASSERT(nth_setup_path(&p) == NTH_RESULT_OK);

    nth_path_set(&p, "first");
    NTH_TEST_ASSERT(inv(&p));
    NTH_TEST_ASSERT(path_is(&p, "first"));

    nth_path_set(&p, "second-and-longer");
    NTH_TEST_ASSERT(inv(&p));
    NTH_TEST_ASSERT(path_is(&p, "second-and-longer"));

    nth_path_set(&p, "s");
    NTH_TEST_ASSERT(inv(&p));
    NTH_TEST_ASSERT(path_is(&p, "s"));

    nth_path_set(&p, "");
    NTH_TEST_ASSERT(inv(&p));
    NTH_TEST_ASSERT(nth_path_empty(&p));

    nth_teardown_path(&p);
    return NTH_TRUE;
}

/* Shrinking then growing again must not leave stale bytes past the new length. */
static nth_b8 test_set_grow_shrink_cycles(void) {
    char big[512];
    NthPath p;

    memset(big, 'x', sizeof big - 1);
    big[sizeof big - 1] = '\0';

    NTH_TEST_ASSERT(nth_setup_path(&p) == NTH_RESULT_OK);

    for (nth_usize i = 0; i < 64; i++) {
        const nth_usize n = 1 + (i * 7) % (sizeof big - 1);

        nth_path_setn(&p, big, n);
        NTH_TEST_ASSERT(inv(&p));
        NTH_TEST_ASSERT(nth_path_length(&p) == n);
        NTH_TEST_ASSERT(memcmp(nth_path_data(&p), big, n) == 0);

        nth_path_set(&p, "tiny");
        NTH_TEST_ASSERT(inv(&p));
        NTH_TEST_ASSERT(path_is(&p, "tiny"));
    }

    nth_teardown_path(&p);
    return NTH_TRUE;
}

static nth_b8 test_setn_truncates(void) {
    char raw[] = "abcdefgh";

    NthPath p;
    NTH_TEST_ASSERT(nth_setup_path(&p) == NTH_RESULT_OK);

    nth_path_setn(&p, raw, 3);
    NTH_TEST_ASSERT(inv(&p));
    NTH_TEST_ASSERT(path_is(&p, "abc"));

    nth_path_setn(&p, raw, 0);
    NTH_TEST_ASSERT(inv(&p));
    NTH_TEST_ASSERT(nth_path_empty(&p));

    nth_teardown_path(&p);
    return NTH_TRUE;
}


/* ================================================================================ */
/*  APPEND                                                                          */
/* ================================================================================ */

/* Appending to an empty path must not introduce a leading separator. */
static nth_b8 test_append_to_empty(void) {
    NthPath p;
    NTH_TEST_ASSERT(nth_setup_path(&p) == NTH_RESULT_OK);

    nth_path_append(&p, "alpha");
    NTH_TEST_ASSERT(inv(&p));
    NTH_TEST_ASSERT(path_is(&p, "alpha"));

    nth_teardown_path(&p);
    return NTH_TRUE;
}

static nth_b8 test_append_empty_is_noop(void) {
    NthPath p;
    NTH_TEST_ASSERT(nth_setup_path_cstr(&p, "alpha") == NTH_RESULT_OK);

    nth_path_append(&p, "");
    NTH_TEST_ASSERT(inv(&p));
    NTH_TEST_ASSERT(path_is(&p, "alpha"));

    nth_path_appendn(&p, "ignored", 0);
    NTH_TEST_ASSERT(inv(&p));
    NTH_TEST_ASSERT(path_is(&p, "alpha"));

    nth_teardown_path(&p);
    return NTH_TRUE;
}

static nth_b8 test_append_joins(void) {
    NthPath p;
    NTH_TEST_ASSERT(nth_setup_path_cstr(&p, "alpha") == NTH_RESULT_OK);

    nth_path_append(&p, "beta");
    NTH_TEST_ASSERT(inv(&p));

#if PATH_APPEND_JOINS
    NTH_TEST_ASSERT(path_is(&p, "alpha" SEP "beta"));
#else
    NTH_TEST_ASSERT(path_is(&p, "alphabeta"));
#endif

    nth_teardown_path(&p);
    return NTH_TRUE;
}

/* Exactly one separator survives, whichever side supplied it. */
#if PATH_APPEND_JOINS
static nth_b8 test_append_no_double_sep(void) {
    NthPath p;

    NTH_TEST_ASSERT(nth_setup_path_cstr(&p, "alpha" SEP) == NTH_RESULT_OK);
    nth_path_append(&p, "beta");
    NTH_TEST_ASSERT(inv(&p));
    NTH_TEST_ASSERT(path_is(&p, "alpha" SEP "beta"));
    nth_teardown_path(&p);

    NTH_TEST_ASSERT(nth_setup_path_cstr(&p, "alpha") == NTH_RESULT_OK);
    nth_path_append(&p, SEP "beta");
    NTH_TEST_ASSERT(inv(&p));
    NTH_TEST_ASSERT(path_is(&p, "alpha" SEP "beta"));
    nth_teardown_path(&p);

    NTH_TEST_ASSERT(nth_setup_path_cstr(&p, "alpha" SEP) == NTH_RESULT_OK);
    nth_path_append(&p, SEP "beta");
    NTH_TEST_ASSERT(inv(&p));
    NTH_TEST_ASSERT(path_is(&p, "alpha" SEP "beta"));
    nth_teardown_path(&p);

    return NTH_TRUE;
}
#endif

static nth_b8 test_appendn_sized(void) {
    char raw[4] = { 'b', 'e', 't', 'a' };

    NthPath p;
    NTH_TEST_ASSERT(nth_setup_path_cstr(&p, "alpha") == NTH_RESULT_OK);

    nth_path_appendn(&p, raw, 3);
    NTH_TEST_ASSERT(inv(&p));

#if PATH_APPEND_JOINS
    NTH_TEST_ASSERT(path_is(&p, "alpha" SEP "bet"));
#else
    NTH_TEST_ASSERT(path_is(&p, "alphabet"));
#endif

    nth_teardown_path(&p);
    return NTH_TRUE;
}

/* Repeated appends across many reallocations must never corrupt earlier bytes. */
static nth_b8 test_append_long_chain(void) {
    NthPath p;
    NTH_TEST_ASSERT(nth_setup_path_cstr(&p, "root") == NTH_RESULT_OK);

    for (nth_usize i = 0; i < LONG_PATH_PARTS; i++) {
        nth_path_append(&p, "seg");
        NTH_TEST_ASSERT(inv(&p));
    }

    NTH_TEST_ASSERT(memcmp(nth_path_data(&p), "root", 4) == 0);
    NTH_TEST_ASSERT(nth_path_length(&p) >= 4 + LONG_PATH_PARTS * 3);
    NTH_TEST_ASSERT(view_is(nth_path_filename(&p), "seg"));

    nth_teardown_path(&p);
    return NTH_TRUE;
}


/* ================================================================================ */
/*  CLEAR / COPY                                                                    */
/* ================================================================================ */

static nth_b8 test_clear_keeps_usable(void) {
    NthPath p;
    NTH_TEST_ASSERT(nth_setup_path_cstr(&p, ABS_DEEP) == NTH_RESULT_OK);

    nth_path_clear(&p);
    NTH_TEST_ASSERT(inv(&p));
    NTH_TEST_ASSERT(nth_path_empty(&p));
    NTH_TEST_ASSERT(nth_path_length(&p) == 0);
    NTH_TEST_ASSERT(path_is(&p, ""));

    nth_path_clear(&p);
    NTH_TEST_ASSERT(inv(&p));
    NTH_TEST_ASSERT(nth_path_empty(&p));

    nth_path_set(&p, "reused");
    NTH_TEST_ASSERT(inv(&p));
    NTH_TEST_ASSERT(path_is(&p, "reused"));

    nth_teardown_path(&p);
    return NTH_TRUE;
}

/* A copy must own its bytes: mutating the source must not disturb the destination. */
static nth_b8 test_copy_is_deep(void) {
    NthPath src, dst;
    NTH_TEST_ASSERT(nth_setup_path_cstr(&src, ABS_DEEP) == NTH_RESULT_OK);
    NTH_TEST_ASSERT(nth_setup_path(&dst) == NTH_RESULT_OK);

    nth_path_copy(&dst, &src);
    NTH_TEST_ASSERT(inv(&dst));
    NTH_TEST_ASSERT(nth_path_equal(&dst, &src));
    NTH_TEST_ASSERT(path_is(&dst, ABS_DEEP));
    NTH_TEST_ASSERT(nth_path_data(&dst) != nth_path_data(&src));

    nth_path_set(&src, "something-else-entirely");
    NTH_TEST_ASSERT(inv(&dst));
    NTH_TEST_ASSERT(path_is(&dst, ABS_DEEP));
    NTH_TEST_ASSERT(!nth_path_equal(&dst, &src));

    nth_teardown_path(&src);

    /* dst outlives the source it was copied from */
    NTH_TEST_ASSERT(inv(&dst));
    NTH_TEST_ASSERT(path_is(&dst, ABS_DEEP));

    nth_teardown_path(&dst);
    return NTH_TRUE;
}

static nth_b8 test_copy_over_existing(void) {
    NthPath src, dst;
    NTH_TEST_ASSERT(nth_setup_path_cstr(&src, "sh") == NTH_RESULT_OK);
    NTH_TEST_ASSERT(nth_setup_path_cstr(&dst, "a-much-longer-destination") == NTH_RESULT_OK);

    nth_path_copy(&dst, &src);
    NTH_TEST_ASSERT(inv(&dst));
    NTH_TEST_ASSERT(path_is(&dst, "sh"));

    nth_path_copy(&src, &dst);
    NTH_TEST_ASSERT(inv(&src));
    NTH_TEST_ASSERT(nth_path_equal(&src, &dst));

    /* copying an empty source clears the destination */
    nth_path_clear(&src);
    nth_path_copy(&dst, &src);
    NTH_TEST_ASSERT(inv(&dst));
    NTH_TEST_ASSERT(nth_path_empty(&dst));

    nth_teardown_path(&src);
    nth_teardown_path(&dst);
    return NTH_TRUE;
}

static nth_b8 test_copy_self(void) {
    NthPath p;
    NTH_TEST_ASSERT(nth_setup_path_cstr(&p, ABS_DEEP) == NTH_RESULT_OK);

    nth_path_copy(&p, &p);
    NTH_TEST_ASSERT(inv(&p));
    NTH_TEST_ASSERT(path_is(&p, ABS_DEEP));

    nth_teardown_path(&p);
    return NTH_TRUE;
}


/* ================================================================================ */
/*  NORMALIZE                                                                       */
/* ================================================================================ */

static const char *const g_norm_inputs[] = {
    "",
    "a",
    "a" SEP "b" SEP "c",
    "a" SEP SEP "b",
    "a" SEP "." SEP "b",
    "." SEP "a",
    "a" SEP "b" SEP ".." SEP "c",
    "a" SEP "b" SEP "..",
    "a" SEP "..",
    "a" SEP ".." SEP "..",
    ".." SEP "a",
    "a" SEP "b" SEP,
    "a" SEP SEP SEP "b" SEP SEP,
    "." ,
    ".." ,
    ABS_ROOT,
    ABS_DEEP,
    ABS_DEEP SEP "." SEP ".." SEP "x",
    ABS_ROOT ".." SEP ".." SEP "x",
    REL_DEEP,
};

#define NORM_COUNT (sizeof g_norm_inputs / sizeof g_norm_inputs[0])

/* Whatever policy normalize implements, running it twice must change nothing the
   second time. This catches the bulk of component-walking bugs without pinning
   any of the debatable outcomes. */
static nth_b8 test_normalize_idempotent(void) {
    for (nth_usize i = 0; i < NORM_COUNT; i++) {
        NthPath a, b;
        NTH_TEST_ASSERT(nth_setup_path_cstr(&a, g_norm_inputs[i]) == NTH_RESULT_OK);

        nth_path_normalize(&a);
        NTH_TEST_ASSERT(inv(&a));

        NTH_TEST_ASSERT(nth_setup_path(&b) == NTH_RESULT_OK);
        nth_path_copy(&b, &a);

        nth_path_normalize(&a);
        NTH_TEST_ASSERT(inv(&a));
        NTH_TEST_ASSERT(nth_path_equal(&a, &b));

        nth_teardown_path(&a);
        nth_teardown_path(&b);
    }
    return NTH_TRUE;
}

/* Normalizing only ever removes components, and never converts an absolute path
   into a relative one or the reverse. */
static nth_b8 test_normalize_invariants(void) {
    for (nth_usize i = 0; i < NORM_COUNT; i++) {
        NthPath p;
        NTH_TEST_ASSERT(nth_setup_path_cstr(&p, g_norm_inputs[i]) == NTH_RESULT_OK);

        const nth_usize before   = nth_path_length(&p);
        const nth_b8    was_abs  = nth_path_is_absolute(&p);

        nth_path_normalize(&p);
        NTH_TEST_ASSERT(inv(&p));
        NTH_TEST_ASSERT(nth_path_length(&p) <= before);
        NTH_TEST_ASSERT((nth_path_is_absolute(&p) != 0) == (was_abs != 0));

        nth_teardown_path(&p);
    }
    return NTH_TRUE;
}

/* The outcomes every reasonable normalize agrees on. */
static nth_b8 test_normalize_known(void) {
    static const char *const cases[][2] = {
        { "a" SEP SEP "b",                "a" SEP "b" },
        { "a" SEP "." SEP "b",            "a" SEP "b" },
        { "a" SEP "b" SEP ".." SEP "c",   "a" SEP "c" },
        { "a" SEP SEP SEP "b",            "a" SEP "b" },
        { "." SEP "a" SEP "b",            "a" SEP "b" },
        { "a" SEP "b" SEP "c",            "a" SEP "b" SEP "c" },
        { ABS_DEEP,                       ABS_DEEP },
        { ABS_ROOT,                       ABS_ROOT },
    };
    const nth_usize count = sizeof cases / sizeof cases[0];

    for (nth_usize i = 0; i < count; i++) {
        NthPath p;
        NTH_TEST_ASSERT(nth_setup_path_cstr(&p, cases[i][0]) == NTH_RESULT_OK);

        nth_path_normalize(&p);
        NTH_TEST_ASSERT(inv(&p));
        NTH_TEST_ASSERT(path_is(&p, cases[i][1]));

        nth_teardown_path(&p);
    }
    return NTH_TRUE;
}

/* ".." must not escape an absolute root. */
static nth_b8 test_normalize_no_escape(void) {
    NthPath p;
    NTH_TEST_ASSERT(nth_setup_path_cstr(&p, ABS_ROOT ".." SEP ".." SEP "tail") == NTH_RESULT_OK);

    nth_path_normalize(&p);
    NTH_TEST_ASSERT(inv(&p));
    NTH_TEST_ASSERT(nth_path_is_absolute(&p));
    NTH_TEST_ASSERT(strstr(nth_path_data(&p), "..") == NULL);

    nth_teardown_path(&p);
    return NTH_TRUE;
}


/* ================================================================================ */
/*  QUERIES                                                                         */
/* ================================================================================ */

static nth_b8 test_absolute_relative(void) {
    static const char *const absolute[] = {
#if NTH_PLATFORM_WINDOWS
        "C:\\", "C:\\Windows", "c:\\x\\y", "\\\\server\\share",
#else
        "/", "/usr", "/usr/local/lib",
#endif
    };
    static const char *const relative[] = {
        "a", "a" SEP "b", "." SEP "a", ".." SEP "a", "file.txt",
    };

    for (nth_usize i = 0; i < sizeof absolute / sizeof absolute[0]; i++) {
        NthPath p;
        NTH_TEST_ASSERT(nth_setup_path_cstr(&p, absolute[i]) == NTH_RESULT_OK);
        NTH_TEST_ASSERT(inv(&p));
        NTH_TEST_ASSERT(nth_path_is_absolute(&p));
        NTH_TEST_ASSERT(!nth_path_is_relative(&p));
        nth_teardown_path(&p);
    }

    for (nth_usize i = 0; i < sizeof relative / sizeof relative[0]; i++) {
        NthPath p;
        NTH_TEST_ASSERT(nth_setup_path_cstr(&p, relative[i]) == NTH_RESULT_OK);
        NTH_TEST_ASSERT(inv(&p));
        NTH_TEST_ASSERT(!nth_path_is_absolute(&p));
        NTH_TEST_ASSERT(nth_path_is_relative(&p));
        nth_teardown_path(&p);
    }

    return NTH_TRUE;
}


/* ================================================================================ */
/*  VIEWS                                                                           */
/* ================================================================================ */

static nth_b8 test_filename(void) {
    static const char *const cases[][2] = {
        { "a" SEP "b" SEP "c.txt", "c.txt" },
        { "c.txt",                 "c.txt" },
        { "a" SEP "b" SEP,         ""      },
        { "",                      ""      },
        { ABS_DEEP SEP "leaf",     "leaf"  },
    };
    const nth_usize count = sizeof cases / sizeof cases[0];

    for (nth_usize i = 0; i < count; i++) {
        NthPath p;
        NTH_TEST_ASSERT(nth_setup_path_cstr(&p, cases[i][0]) == NTH_RESULT_OK);

        NTH_TEST_ASSERT(inv(&p));
        NTH_TEST_ASSERT(view_is(nth_path_filename(&p), cases[i][1]));
        NTH_TEST_ASSERT(check_dir_filename(&p));

        nth_teardown_path(&p);
    }
    return NTH_TRUE;
}

static nth_b8 test_directory(void) {
    static const char *const cases[] = {
        "a" SEP "b" SEP "c.txt",
        "c.txt",
        "a" SEP "b" SEP,
        "",
        ABS_DEEP SEP "leaf",
        ABS_ROOT "leaf",
    };
    const nth_usize count = sizeof cases / sizeof cases[0];

    for (nth_usize i = 0; i < count; i++) {
        NthPath p;
        NTH_TEST_ASSERT(nth_setup_path_cstr(&p, cases[i]) == NTH_RESULT_OK);

        NTH_TEST_ASSERT(inv(&p));
        NTH_TEST_ASSERT(check_dir_filename(&p));

        nth_teardown_path(&p);
    }

    /* a path with no separator has no directory */
    NthPath p;
    NTH_TEST_ASSERT(nth_setup_path_cstr(&p, "loose.txt") == NTH_RESULT_OK);
    NTH_TEST_ASSERT(nth_path_directory(&p).size == 0);
    nth_teardown_path(&p);

    return NTH_TRUE;
}

static nth_b8 test_stem_extension(void) {
    static const char *const cases[] = {
        "a" SEP "b" SEP "c.txt",
        "archive.tar.gz",
        "noext",
        "a" SEP "noext",
        ".bashrc",
        "trailing.",
        "a" SEP "b" SEP,
        "",
        ABS_DEEP SEP "leaf.so",
    };
    const nth_usize count = sizeof cases / sizeof cases[0];

    for (nth_usize i = 0; i < count; i++) {
        NthPath p;
        NTH_TEST_ASSERT(nth_setup_path_cstr(&p, cases[i]) == NTH_RESULT_OK);

        NTH_TEST_ASSERT(inv(&p));
        NTH_TEST_ASSERT(check_split(&p));

        nth_teardown_path(&p);
    }
    return NTH_TRUE;
}

/* The unambiguous half of the extension rules: a name with no dot has no
   extension, and the extension comes from the last dot, not the first. */
static nth_b8 test_extension_known(void) {
    NthPath p;

    NTH_TEST_ASSERT(nth_setup_path_cstr(&p, "a" SEP "noext") == NTH_RESULT_OK);
    NTH_TEST_ASSERT(nth_path_extension(&p).size == 0);
    NTH_TEST_ASSERT(view_is(nth_path_stem(&p), "noext"));
    nth_teardown_path(&p);

    NTH_TEST_ASSERT(nth_setup_path_cstr(&p, "archive.tar.gz") == NTH_RESULT_OK);
    NTH_TEST_ASSERT(view_is(nth_path_stem(&p), "archive.tar"));
#if PATH_EXT_INCLUDES_DOT
    NTH_TEST_ASSERT(view_is(nth_path_extension(&p), ".gz"));
#else
    NTH_TEST_ASSERT(view_is(nth_path_extension(&p), "gz"));
#endif
    nth_teardown_path(&p);

    /* the directory's dot must not leak into the filename's extension */
    NTH_TEST_ASSERT(nth_setup_path_cstr(&p, "v1.2" SEP "noext") == NTH_RESULT_OK);
    NTH_TEST_ASSERT(nth_path_extension(&p).size == 0);
    nth_teardown_path(&p);

    return NTH_TRUE;
}

/* Views must survive being taken from a path that is then read, and must track
   the buffer after a mutation reallocates it. */
static nth_b8 test_views_after_mutation(void) {
    NthPath p;
    NTH_TEST_ASSERT(nth_setup_path_cstr(&p, "a" SEP "b.txt") == NTH_RESULT_OK);
    NTH_TEST_ASSERT(view_is(nth_path_filename(&p), "b.txt"));

    nth_path_set(&p, "much" SEP "longer" SEP "path" SEP "to" SEP "c.bin");
    NTH_TEST_ASSERT(inv(&p));
    NTH_TEST_ASSERT(view_is(nth_path_filename(&p), "c.bin"));

    nth_path_clear(&p);
    NTH_TEST_ASSERT(nth_path_filename(&p).size == 0);
    NTH_TEST_ASSERT(nth_path_directory(&p).size == 0);
    NTH_TEST_ASSERT(nth_path_stem(&p).size == 0);
    NTH_TEST_ASSERT(nth_path_extension(&p).size == 0);

    nth_teardown_path(&p);
    return NTH_TRUE;
}


/* ================================================================================ */
/*  EQUALITY                                                                        */
/* ================================================================================ */

static nth_b8 test_equal_basics(void) {
    NthPath a, b, c;
    NTH_TEST_ASSERT(nth_setup_path_cstr(&a, ABS_DEEP) == NTH_RESULT_OK);
    NTH_TEST_ASSERT(nth_setup_path_cstr(&b, ABS_DEEP) == NTH_RESULT_OK);
    NTH_TEST_ASSERT(nth_setup_path_cstr(&c, ABS_DEEP "x") == NTH_RESULT_OK);

    NTH_TEST_ASSERT(nth_path_equal(&a, &a));
    NTH_TEST_ASSERT(nth_path_equal(&a, &b));
    NTH_TEST_ASSERT(nth_path_equal(&b, &a));
    NTH_TEST_ASSERT(!nth_path_equal(&a, &c));
    NTH_TEST_ASSERT(!nth_path_equal(&c, &a));

    nth_teardown_path(&a);
    nth_teardown_path(&b);
    nth_teardown_path(&c);
    return NTH_TRUE;
}

/* A prefix is not a match: catches comparisons that stop at the shorter length. */
static nth_b8 test_equal_prefix_rejected(void) {
    NthPath a, b;
    NTH_TEST_ASSERT(nth_setup_path_cstr(&a, "abc") == NTH_RESULT_OK);
    NTH_TEST_ASSERT(nth_setup_path_cstr(&b, "abcd") == NTH_RESULT_OK);

    NTH_TEST_ASSERT(!nth_path_equal(&a, &b));
    NTH_TEST_ASSERT(!nth_path_equal(&b, &a));

    nth_path_set(&b, "abd");
    NTH_TEST_ASSERT(!nth_path_equal(&a, &b));

    nth_path_set(&b, "abc");
    NTH_TEST_ASSERT(nth_path_equal(&a, &b));

    nth_teardown_path(&a);
    nth_teardown_path(&b);
    return NTH_TRUE;
}

static nth_b8 test_equal_empty(void) {
    NthPath a, b;
    NTH_TEST_ASSERT(nth_setup_path(&a) == NTH_RESULT_OK);
    NTH_TEST_ASSERT(nth_setup_path_cstr(&b, "x") == NTH_RESULT_OK);

    nth_path_clear(&b);
    NTH_TEST_ASSERT(nth_path_equal(&a, &b));
    NTH_TEST_ASSERT(nth_path_equal_view(&a, view_make(NULL, 0)));

    nth_teardown_path(&a);
    nth_teardown_path(&b);
    return NTH_TRUE;
}

static nth_b8 test_equal_view_forms_agree(void) {
    char raw[] = "alpha" SEP "beta" SEP "gamma";
    const nth_usize n = strlen(raw);

    NthPath p;
    NTH_TEST_ASSERT(nth_setup_path_cstr(&p, raw) == NTH_RESULT_OK);

    const NthPathView full  = view_make(raw, n);
    const NthPathView short_ = view_make(raw, n - 1);

    NTH_TEST_ASSERT(nth_path_equal_view(&p, full));
    NTH_TEST_ASSERT(!nth_path_equal_view(&p, short_));

    NTH_TEST_ASSERT(nth_path_view_equal(full, full));
    NTH_TEST_ASSERT(!nth_path_view_equal(full, short_));
    NTH_TEST_ASSERT(nth_path_view_equal(view_make(NULL, 0), view_make(raw, 0)));

    NTH_TEST_ASSERT(nth_path_view_equal_cstr(full, raw));
    NTH_TEST_ASSERT(!nth_path_view_equal_cstr(short_, raw));
    NTH_TEST_ASSERT(nth_path_view_equal_cstr(view_make(NULL, 0), ""));

    nth_teardown_path(&p);
    return NTH_TRUE;
}

/* The view compares size bytes, not up to a NUL in the source. */
static nth_b8 test_view_equal_cstr_length_bound(void) {
    char raw[] = "abcdef";
    const NthPathView v = view_make(raw, 3);

    NTH_TEST_ASSERT(nth_path_view_equal_cstr(v, "abc"));
    NTH_TEST_ASSERT(!nth_path_view_equal_cstr(v, "abcdef"));
    NTH_TEST_ASSERT(!nth_path_view_equal_cstr(v, "ab"));
    NTH_TEST_ASSERT(!nth_path_view_equal_cstr(v, ""));
    NTH_TEST_ASSERT(!nth_path_view_equal_cstr(v, "abd"));

    return NTH_TRUE;
}


/* ================================================================================ */
/*  SYSTEM PATHS                                                                    */
/* ================================================================================ */

/* cwd and exe are backed by syscalls that cannot legitimately fail on either
   platform, so those must come back absolute. config and data depend on the
   environment, which a stripped CI container may not provide, so they are only
   required to be well formed. */
static nth_b8 test_system_paths(void) {
    NthPath cwd, exe, cfg, dat;
    NTH_TEST_ASSERT(nth_setup_path(&cwd) == NTH_RESULT_OK);
    NTH_TEST_ASSERT(nth_setup_path(&exe) == NTH_RESULT_OK);
    NTH_TEST_ASSERT(nth_setup_path(&cfg) == NTH_RESULT_OK);
    NTH_TEST_ASSERT(nth_setup_path(&dat) == NTH_RESULT_OK);

    nth_path_system_cwd(&cwd);
    nth_path_system_exe(&exe);
    nth_path_system_config(&cfg);
    nth_path_system_data(&dat);

    NTH_TEST_ASSERT(inv(&cwd));
    NTH_TEST_ASSERT(inv(&exe));
    NTH_TEST_ASSERT(inv(&cfg));
    NTH_TEST_ASSERT(inv(&dat));

    NTH_TEST_ASSERT(!nth_path_empty(&cwd));
    NTH_TEST_ASSERT(nth_path_is_absolute(&cwd));

    NTH_TEST_ASSERT(!nth_path_empty(&exe));
    NTH_TEST_ASSERT(nth_path_is_absolute(&exe));

    if (!nth_path_empty(&cfg))
        NTH_TEST_ASSERT(nth_path_is_absolute(&cfg));
    if (!nth_path_empty(&dat))
        NTH_TEST_ASSERT(nth_path_is_absolute(&dat));

    nth_teardown_path(&cwd);
    nth_teardown_path(&exe);
    nth_teardown_path(&cfg);
    nth_teardown_path(&dat);
    return NTH_TRUE;
}

/* Repeated calls are stable and overwrite rather than accumulate. */
static nth_b8 test_system_paths_stable(void) {
    NthPath a, b;
    NTH_TEST_ASSERT(nth_setup_path(&a) == NTH_RESULT_OK);
    NTH_TEST_ASSERT(nth_setup_path_cstr(&b, "leftover-content-to-overwrite") == NTH_RESULT_OK);

    nth_path_system_cwd(&a);
    nth_path_system_cwd(&b);
    NTH_TEST_ASSERT(inv(&b));
    NTH_TEST_ASSERT(nth_path_equal(&a, &b));

    nth_path_system_exe(&a);
    nth_path_system_exe(&b);
    NTH_TEST_ASSERT(nth_path_equal(&a, &b));

    /* the exe directory is a directory, so appending a name yields a child */
    nth_path_append(&b, "child.bin");
    NTH_TEST_ASSERT(inv(&b));
    NTH_TEST_ASSERT(view_is(nth_path_filename(&b), "child.bin"));

    nth_teardown_path(&a);
    nth_teardown_path(&b);
    return NTH_TRUE;
}


/* ================================================================================ */
/*  STRESS                                                                          */
/* ================================================================================ */

#define MODEL_MAX 8192

/* Components never contain a separator and are never empty, so the join rule is
   unambiguous: exactly one separator between two non-empty parts. */
static const char *const g_parts[] = {
    "a", "bb", "ccc", "dddd", "seg", "component", "x1", "node.txt"
};
#define PART_COUNT (sizeof g_parts / sizeof g_parts[0])

static void model_append(char *m, nth_usize *mlen, const char *s) {
    const nth_usize n = strlen(s);

#if PATH_APPEND_JOINS
    if (*mlen != 0)
        m[(*mlen)++] = SEPC;
#endif

    memcpy(m + *mlen, s, n);
    *mlen += n;
    m[*mlen] = '\0';
}

/* Drives the mutators against a plain char buffer holding the expected value and
   compares after every step, so a divergence is caught at the operation that
   caused it rather than at the end. */
static nth_b8 test_stress_model(void) {
    char      model[MODEL_MAX];
    nth_usize mlen = 0;

    NthPath p, scratch;
    NTH_TEST_ASSERT(nth_setup_path(&p) == NTH_RESULT_OK);
    NTH_TEST_ASSERT(nth_setup_path(&scratch) == NTH_RESULT_OK);

    model[0] = '\0';

    nth_usize appends = 0, sets = 0, clears = 0, copies = 0;

    for (nth_usize it = 0; it < STRESS_IT; it++) {
        const unsigned op = rnd() % 100;

        if (op < 55) {
            const char *part = g_parts[rnd() % PART_COUNT];

            if (mlen + strlen(part) + 2 >= MODEL_MAX) {
                nth_path_clear(&p);
                model[0] = '\0';
                mlen     = 0;
                clears++;
                continue;
            }

            nth_path_append(&p, part);
            model_append(model, &mlen, part);
            appends++;
        } else if (op < 75) {
            const char *part = g_parts[rnd() % PART_COUNT];

            nth_path_set(&p, part);
            mlen = strlen(part);
            memcpy(model, part, mlen + 1);
            sets++;
        } else if (op < 85) {
            nth_path_clear(&p);
            model[0] = '\0';
            mlen     = 0;
            clears++;
        } else {
            /* round trip through a second path */
            nth_path_copy(&scratch, &p);
            NTH_TEST_ASSERT(nth_path_equal(&scratch, &p));

            nth_path_clear(&p);
            nth_path_copy(&p, &scratch);
            copies++;
        }

        NTH_TEST_ASSERT(inv(&p));
        NTH_TEST_ASSERT(nth_path_length(&p) == mlen);
        NTH_TEST_ASSERT(memcmp(nth_path_data(&p), model, mlen) == 0);
        NTH_TEST_ASSERT(nth_path_equal_view(&p, view_make(model, mlen)));
    }

    NTH_TEST_ASSERT(appends > 1000);
    NTH_TEST_ASSERT(sets > 100);
    NTH_TEST_ASSERT(clears > 100);
    NTH_TEST_ASSERT(copies > 100);

    nth_teardown_path(&p);
    nth_teardown_path(&scratch);
    return NTH_TRUE;
}

/* Many independent paths alive at once, torn down out of order. */
static nth_b8 test_stress_many_live(void) {
    enum { LIVE = 256 };

    NthPath  paths[LIVE];
    char     want[LIVE][64];

    for (nth_usize i = 0; i < LIVE; i++) {
        NTH_TEST_ASSERT(nth_setup_path(&paths[i]) == NTH_RESULT_OK);

        nth_usize n = 0;
        want[i][0] = '\0';

        const nth_usize depth = 1 + (i % 5);
        for (nth_usize k = 0; k < depth; k++) {
            const char *part = g_parts[(i + k) % PART_COUNT];
            nth_path_append(&paths[i], part);
            model_append(want[i], &n, part);
        }
    }

    for (nth_usize i = 0; i < LIVE; i++) {
        NTH_TEST_ASSERT(inv(&paths[i]));
        NTH_TEST_ASSERT(path_is(&paths[i], want[i]));
    }

    /* tear down odds first, then evens, so frees interleave with live objects */
    for (nth_usize i = 1; i < LIVE; i += 2)
        nth_teardown_path(&paths[i]);

    for (nth_usize i = 0; i < LIVE; i += 2) {
        NTH_TEST_ASSERT(path_is(&paths[i], want[i]));
        nth_teardown_path(&paths[i]);
    }

    return NTH_TRUE;
}


/* ================================================================================ */

int main(void) {
    if (nth_init(NULL) != NTH_RESULT_OK) {
        fprintf(stderr, "nth_init failed\n");
        return EXIT_FAILURE;
    }

    NthTest tests[] = {
        { "setup/empty",              test_setup_empty              },
        { "setup/str",                test_setup_str                },
        { "setup/str_empty",          test_setup_str_empty          },
        { "setup/view_unterminated",  test_setup_view_unterminated  },
        { "setup/view_empty",         test_setup_view_empty         },
        { "teardown/reuse",           test_teardown_then_reuse      },

        { "set/replaces",             test_set_replaces             },
        { "set/grow_shrink_cycles",   test_set_grow_shrink_cycles   },
        { "set/setn_truncates",       test_setn_truncates           },

        { "append/to_empty",          test_append_to_empty          },
        { "append/empty_is_noop",     test_append_empty_is_noop     },
        { "append/joins",             test_append_joins             },
#if PATH_APPEND_JOINS
        { "append/no_double_sep",     test_append_no_double_sep     },
#endif
        { "append/sized",             test_appendn_sized            },
        { "append/long_chain",        test_append_long_chain        },

        { "clear/keeps_usable",       test_clear_keeps_usable       },
        { "copy/is_deep",             test_copy_is_deep             },
        { "copy/over_existing",       test_copy_over_existing       },
        { "copy/self",                test_copy_self                },

        { "normalize/idempotent",     test_normalize_idempotent     },
        { "normalize/invariants",     test_normalize_invariants     },
        { "normalize/known",          test_normalize_known          },
        { "normalize/no_escape",      test_normalize_no_escape      },

        { "query/absolute_relative",  test_absolute_relative        },

        { "view/filename",            test_filename                 },
        { "view/directory",           test_directory                },
        { "view/stem_extension",      test_stem_extension           },
        { "view/extension_known",     test_extension_known          },
        { "view/after_mutation",      test_views_after_mutation     },

        { "equal/basics",             test_equal_basics             },
        { "equal/prefix_rejected",    test_equal_prefix_rejected    },
        { "equal/empty",              test_equal_empty              },
        { "equal/view_forms_agree",   test_equal_view_forms_agree   },
        { "equal/cstr_length_bound",  test_view_equal_cstr_length_bound },

        { "system/paths",             test_system_paths             },
        { "system/paths_stable",      test_system_paths_stable      },

        { "stress/model",             test_stress_model             },
        { "stress/many_live",         test_stress_many_live         },
    };

    const int rc = NTH_RUN_TESTS(tests);

    nth_term();
    return rc;
}
