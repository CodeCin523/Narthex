#include <narthex/narthex.h>

#include "lifecycle.h"
#include "log/log_internal.h"


static volatile NthLifecycle g_core;


NthResult nth_init(const NthCoreDesc *desc) {
    NthResult r = nth_lifecycle_begin_init(&g_core);
    if (r != NTH_RESULT_OK)
        return r;

    r = nth_init_log(desc != NULL ? &desc->logger : NULL);
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

    nth_term_log();

    nth_lifecycle_end_term(&g_core);
}
