#include <narthex/narthex.h>

#include "lifecycle.h"


static volatile NthLifecycle g_core;


/* Neither may run concurrently with nth_log, nth_logf or nth_flush. */
NthResult nth_init_log(const NthLoggerDesc *desc);
void nth_term_log(void);

NthResult nth_init_path(const char *app_name);
void nth_term_path(void);

NthResult nth_init_uptime();
void nth_term_uptime();


NthResult nth_init(const NthCoreDesc *desc) {
    NthResult r = nth_lifecycle_begin_init(&g_core);
    if (r != NTH_RESULT_OK)
        return r;

    r = nth_init_log(desc != NULL ? &desc->logger : NULL);
    if (r != NTH_RESULT_OK) {
        nth_lifecycle_fail_init(&g_core);
        return r;
    }
    r = nth_init_path(desc != NULL? desc->app_name : NULL);
    if (r != NTH_RESULT_OK) {
        nth_lifecycle_fail_init(&g_core);
        return r;
    }
    r = nth_init_uptime();
    if (r != NTH_RESULT_OK) {
        nth_lifecycle_fail_init(&g_core);
        return r;
    }

    nth_lifecycle_end_init(&g_core);
    return NTH_RESULT_OK;
}

void nth_term(void) {
    if (nth_lifecycle_begin_term(&g_core) != NTH_RESULT_OK)
        return;

    nth_term_uptime();
    nth_term_path();
    nth_term_log();

    nth_lifecycle_end_term(&g_core);
}
