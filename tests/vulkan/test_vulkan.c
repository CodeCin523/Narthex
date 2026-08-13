#include <narthex/nth_config.h>
#include <stdlib.h>

#if !NTH_HAS_VULKAN
#error "test_vulkan built without vulkan module"
#endif

int main(void) {
    return EXIT_SUCCESS;
}
