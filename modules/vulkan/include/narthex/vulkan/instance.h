#ifndef NTH_VULKAN_INSTANCE_H
#define NTH_VULKAN_INSTANCE_H

#include <narthex/nth_config.h>

#if !NTH_HAS_VULKAN
#error "Narthex was built without the vulkan module"
#endif

#include <narthex/nth_types.h>
#include <narthex/nth_result.h>
#include <narthex/utils/api.h>

#include <vulkan/vulkan.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NthVulkanDesc {
    const char *app_name;
    nth_u32 app_version;
    const char *engine_name;
    nth_u32 engine_version;

    const char **extensions;
    nth_u32 extension_count;

    nth_b8 enable_naming;
    nth_b8 enable_validation;
} NthVulkanDesc;


NTH_API NthResult nth_init_vulkan(NthVulkanDesc *desc);
NTH_API void nth_term_vulkan();

NTH_API VkInstance nth_vulkan_get_instance(void);


#ifdef __cplusplus
}
#endif

#endif /* NTH_VULKAN_INSTANCE_H */