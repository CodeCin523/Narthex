#ifndef NTH_TEST_H
#define NTH_TEST_H

#include <narthex/nth_types.h>
#include <stdio.h>
#include <stdlib.h>


#define NTH_TEST_ASSERT(expr) \
    do { if (!(expr)) { \
        fprintf(stderr, "  assert failed: %s (%s:%d)\n", #expr, __func__, __LINE__); \
        return NTH_FALSE; \
    }} while(0)

typedef b8 (*NthTestFn)(void);

typedef struct {
    const char *name;
    NthTestFn   fn;
} NthTest;

static inline int nth_run_tests(NthTest *tests, usize count) {
    usize failed = 0;
    for (usize i = 0; i < count; i++) {
        if (tests[i].fn()) {
            fprintf(stdout, "PASS  %s\n", tests[i].name);
        } else {
            fprintf(stderr, "FAIL  %s\n", tests[i].name);
            failed++;
        }
    }
    if (failed)
        fprintf(stderr, "\n%zu/%zu failed\n", failed, count);
    return failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

#define NTH_RUN_TESTS(tests) nth_run_tests(tests, sizeof(tests) / sizeof(tests[0]))


#endif /* NTH_TEST_H */
