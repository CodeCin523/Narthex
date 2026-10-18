#ifndef NTH_VULKAN_H
#define NTH_VULKAN_H

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


#define NTH_VULKAN_MAX_QUEUES 8


typedef struct NthVulkanQueue {
    VkQueue queue;
    nth_u32 family;
    VkQueueFlags flags;
    nth_b8 supports_present;
} NthVulkanQueue;
typedef struct NthVulkanDevice {
    VkPhysicalDevice phy_device;
    VkDevice device;

    NthVulkanQueue queues[NTH_VULKAN_MAX_QUEUES];
    nth_u32 queue_count;
} NthVulkanDevice;

typedef struct NthVulkanDesc {
    const char *app_name;
    nth_u32 app_version;
    const char *engine_name;
    nth_u32 engine_version;

    const char **extensions;
    nth_u32 extension_count;

    nth_b8 enable_validation;
} NthVulkanDesc;
typedef struct NthVulkanDeviceDesc {
    nth_i32 (*pfn_score_device)(VkPhysicalDevice);

    VkPhysicalDeviceFeatures2 *features;

    const char **extensions;
    nth_u32 extension_count;

    VkSurfaceKHR surface;
} NthVulkanDeviceDesc;


NTH_API NthResult nth_init_vulkan(NthVulkanDesc *desc);
NTH_API void nth_term_vulkan();

NTH_API NthResult nth_setup_vulkan_device(NthVulkanDevice *device, NthVulkanDeviceDesc *desc);
NTH_API void nth_teardown_vulkan_device(NthVulkanDevice *device);


#ifdef __cplusplus
}
#endif

#endif /* NTH_VULKAN_H */
