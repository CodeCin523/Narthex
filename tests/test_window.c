#include <narthex/nth_config.h>
#include <stdlib.h>

#if !NTH_HAS_WINDOW
#error "test_window built without window module"
#endif

int main(void) {
    return EXIT_SUCCESS;
}
