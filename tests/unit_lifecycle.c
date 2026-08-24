#include <narthex/narthex.h>
#include <narthex/nth_log.h>

#include <stdio.h>

int main(void) {
    NthResult r;

    for (int i = 0; i < 1000; ++i) {
        fprintf(stderr, "init #%d...\n", i + 1);

        r = nth_init(NULL);
        if (r != NTH_RESULT_OK) {
            fprintf(stderr, "init #%d failed: %d\n", i + 1, r);
            return 1;
        }

        nth_log(NTH_LOG_LEVEL_INFO, "hi");
        nth_logf(NTH_LOG_LEVEL_INFO, "Narthex initialized (cycle #%d)", i + 1);

        fprintf(stderr, "term #%d...\n", i + 1);
        nth_term();
    }

    fprintf(stderr, "lifecycle test passed: 1000 cycles\n");
    return 0;
}