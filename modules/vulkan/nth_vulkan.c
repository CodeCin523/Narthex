#include <narthex/nth_vulkan.h>

#include <narthex/utils/build.h>
#include <narthex/utils/check.h>
#include <narthex/nth_log.h>

#include <stdlib.h>
#include <string.h>


static const char *const VALIDATION_LAYERS[] = {
    "VK_LAYER_KHRONOS_validation"
};
static const nth_u32 VALIDATION_LAYER_COUNT = sizeof(VALIDATION_LAYERS) / sizeof(VALIDATION_LAYERS[0]);

static struct {
    VkInstance instance;
    VkDebugUtilsMessengerEXT messenger;

    VkPhysicalDevice *phy_devices;
    nth_u32 phy_device_count;
} g_vulkan = {0};


static VKAPI_ATTR VkBool32 VKAPI_CALL debug_messenger_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT type,
    const VkDebugUtilsMessengerCallbackDataEXT *data,
    void *user_data
) {
    (void)type;
    (void)user_data;

    if(severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
        NTH_LOG_ERROR(data->pMessage);
    else if(severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        NTH_LOG_WARN(data->pMessage);
    #if NTH_DEBUG
    else
        NTH_LOG_DEBUG(data->pMessage);
    #endif

    return VK_FALSE;
}
static void fill_debug_messenger_info(VkDebugUtilsMessengerCreateInfoEXT *info) {
    *info = (VkDebugUtilsMessengerCreateInfoEXT){
        .sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = debug_messenger_callback,
    };
}

nth_i32 default_device_score(VkPhysicalDevice device) {
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(device, &props);

    switch(props.deviceType) {
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
            return 1000;
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
            return 100;
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
            return 10;
        default:
            return 0;
    }
}

nth_i32 queue_priority(VkQueueFlags flags, nth_b8 supports_present) {
    if((flags & VK_QUEUE_GRAPHICS_BIT) && supports_present) return 0;
    if(flags & VK_QUEUE_GRAPHICS_BIT) return 1;
    if(flags & VK_QUEUE_COMPUTE_BIT) return 2;
    return 3;
}


/* ================================================================================ */
/*  IMPLEMENTATION                                                                  */
/* ================================================================================ */

NTH_API NthResult nth_init_vulkan(NthVulkanDesc *desc) {
    NTH_DASSERT(NTH_LIKELY(desc != NULL));
    NTH_DASSERT(NTH_LIKELY(desc->app_name != NULL));
    NTH_DASSERT(NTH_LIKELY(desc->engine_name != NULL));

    desc->enable_validation = NTH_DEBUG ? 1 : desc->enable_validation;

    if(desc->enable_validation) {
        nth_u32 layer_count = 0;
        vkEnumerateInstanceLayerProperties(&layer_count, NULL);
        VkLayerProperties *available = malloc(layer_count * sizeof(VkLayerProperties));
        if(!available)
            return NTH_RESULT_OUT_OF_MEMORY;
        vkEnumerateInstanceLayerProperties(&layer_count, available);
    
        for(nth_u32 i = 0; i < VALIDATION_LAYER_COUNT; ++i) {
            nth_b8 found = NTH_FALSE;
            for(nth_u32 j = 0; j < layer_count; ++j) {
                if(strcmp(VALIDATION_LAYERS[i], available[j].layerName) == 0) {
                    found = NTH_TRUE;
                    break;
                }
            }
            if(found == NTH_FALSE) {
                free(available);
                NTH_LOG_FATAL("Required validation layer not available");
                return NTH_RESULT_FAILED;
            }
        }

        free(available);
    }

    nth_u32 ext_count = desc->extension_count;
    const char **extensions = malloc((ext_count + 1) * sizeof(const char *));
    if(!extensions)
        return NTH_RESULT_OUT_OF_MEMORY;

    for(nth_u32 i = 0; i < ext_count; ++i)
        extensions[i] = desc->extensions[i];
    if(desc->enable_validation)
        extensions[ext_count++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;

    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = desc->app_name,
        .applicationVersion = desc->app_version,
        .pEngineName = desc->engine_name,
        .engineVersion = desc->engine_version,
        .apiVersion = VK_API_VERSION_1_3
    };

    VkDebugUtilsMessengerCreateInfoEXT debug_info = {0};
    if(desc->enable_validation)
        fill_debug_messenger_info(&debug_info);

    VkInstanceCreateInfo instance_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = desc->enable_validation? &debug_info : NULL,
        .pApplicationInfo = &app_info,
        .ppEnabledLayerNames = desc->enable_validation? VALIDATION_LAYERS : 0,
        .enabledLayerCount = desc->enable_validation? VALIDATION_LAYER_COUNT : 0,
        .ppEnabledExtensionNames = extensions,
        .enabledExtensionCount = ext_count
    };

    VkResult res = vkCreateInstance(&instance_info, NULL, &g_vulkan.instance);
    
    free(extensions);
    if(res != VK_SUCCESS) {
        NTH_LOG_FATAL("Failed to create vulkan instance");
        return NTH_RESULT_FAILED;
    }

    if(desc->enable_validation) {
        PFN_vkCreateDebugUtilsMessengerEXT fn = (PFN_vkCreateDebugUtilsMessengerEXT)
            vkGetInstanceProcAddr(g_vulkan.instance, "vkCreateDebugUtilsMessengerEXT");

        if(!fn || fn(g_vulkan.instance, &debug_info, NULL, &g_vulkan.messenger) != VK_SUCCESS) {
            NTH_LOG_FATAL("Failed to create debug messenger");
            vkDestroyInstance(g_vulkan.instance, NULL);
            g_vulkan.instance = VK_NULL_HANDLE;
            return NTH_RESULT_FAILED;
        }
    }

    vkEnumeratePhysicalDevices(g_vulkan.instance, &g_vulkan.phy_device_count, NULL);
    if(g_vulkan.phy_device_count == 0) {
        NTH_LOG_FATAL("No vulkan-capable GPU found");
        vkDestroyInstance(g_vulkan.instance, NULL);
        g_vulkan.instance = VK_NULL_HANDLE;
        return NTH_RESULT_FAILED;
    }

    g_vulkan.phy_devices = malloc(g_vulkan.phy_device_count * sizeof(VkPhysicalDevice));
    if(!g_vulkan.phy_device_count) {
        vkDestroyInstance(g_vulkan.instance, NULL);
        g_vulkan.instance = VK_NULL_HANDLE;
        return NTH_RESULT_OUT_OF_MEMORY;
    }

    vkEnumeratePhysicalDevices(g_vulkan.instance, &g_vulkan.phy_device_count, g_vulkan.phy_devices);

    NTH_LOG_INFO("Vulkan instance created");
    return NTH_RESULT_OK;
}
NTH_API void nth_term_vulkan() {
    if(g_vulkan.phy_devices) {
        free(g_vulkan.phy_devices);
        g_vulkan.phy_device_count = 0;
    }

    if(g_vulkan.messenger != VK_NULL_HANDLE) {
        PFN_vkDestroyDebugUtilsMessengerEXT fn = (PFN_vkDestroyDebugUtilsMessengerEXT)
            vkGetInstanceProcAddr(g_vulkan.instance, "vkDestroyDebugUtilsMessengerEXT");
        if(fn)
            fn(g_vulkan.instance, g_vulkan.messenger, NULL);
        g_vulkan.messenger = VK_NULL_HANDLE;
    }

    if(g_vulkan.instance != VK_NULL_HANDLE) {
        vkDestroyInstance(g_vulkan.instance, NULL);
        g_vulkan.instance = VK_NULL_HANDLE;
    }
}

NTH_API NthResult nth_setup_vulkan_device(NthVulkanDevice *device, NthVulkanDeviceDesc *desc) {
    NTH_DASSERT(NTH_LIKELY(device != NULL));
    NTH_DASSERT(NTH_LIKELY(desc != NULL));

    nth_i32 (*pfn_score)(VkPhysicalDevice) = desc->pfn_score_device? desc->pfn_score_device : default_device_score;
    
    VkPhysicalDevice best_device = VK_NULL_HANDLE;
    nth_i32 best_score = 0;
    nth_u32 best_index = 0;

    for(nth_u32 i = 0; i < g_vulkan.phy_device_count; ++i) {
        nth_i32 score = pfn_score(g_vulkan.phy_devices[i]);
        if(score > best_score) {
            best_score = score;
            best_device = g_vulkan.phy_devices[i];
            best_index = i;
        }
    }

    if(best_device == VK_NULL_HANDLE) {
        NTH_LOG_FATAL("No suitable GPU available");
        return NTH_RESULT_FAILED;
    }

    g_vulkan.phy_devices[best_index] = g_vulkan.phy_devices[--g_vulkan.phy_device_count];
    device->phy_device = best_device;

    if(desc->surface != VK_NULL_HANDLE) {
        nth_u32 ext_count = 0;
        vkEnumerateDeviceExtensionProperties(best_device, NULL, &ext_count, NULL);
        VkExtensionProperties *exts = malloc(ext_count * sizeof(VkExtensionProperties));
        if(!exts)
            return NTH_RESULT_OUT_OF_MEMORY;

        vkEnumerateDeviceExtensionProperties(best_device, NULL, &ext_count, exts);

        nth_b8 has_swapchain = 0;
        for(nth_u32 i = 0; i < ext_count; ++i) {
            if(strcmp(exts[i].extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
                has_swapchain = 1;
                break;
            }
        }
        free(exts);

        if(!has_swapchain) {
            NTH_LOG_FATAL("Device does not support VK_KHR_swapchain");
            return NTH_RESULT_FAILED;
        }
    }

    nth_u32 family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(best_device, &family_count, NULL);
    VkQueueFamilyProperties *families = malloc(family_count * sizeof(VkQueueFamilyProperties));
    if(!families)
        return NTH_RESULT_OUT_OF_MEMORY;

    vkGetPhysicalDeviceQueueFamilyProperties(best_device, &family_count, families);

    NthVulkanQueue tmp[NTH_VULKAN_MAX_QUEUES];
    nth_u32 tmp_count = 0;

    for(nth_u32 i = 0; i < family_count && tmp_count < NTH_VULKAN_MAX_QUEUES; ++i) {
        VkQueueFlags flags = families[i].queueFlags;

        if(!(flags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT)))
            continue;

        VkBool32 present = VK_FALSE;
        if(desc->surface != VK_NULL_HANDLE)
            vkGetPhysicalDeviceSurfaceSupportKHR(best_device, i, desc->surface, &present);

        tmp[tmp_count++] = (NthVulkanQueue){
            .queue            = VK_NULL_HANDLE,
            .family           = i,
            .flags            = flags,
            .supports_present = (nth_b8)present
        };
    }
    free(families);

    if(desc->surface != VK_NULL_HANDLE) {
        nth_b8 found_present = NTH_FALSE;
        for(nth_u32 i = 0; i < tmp_count; ++i)
            if(tmp[i].supports_present) {
                found_present = NTH_TRUE;
                break;
            }

        if(!found_present) {
            NTH_LOG_FATAL("No queue family supports present for this surface");
            return NTH_RESULT_FAILED;
        }
    }

    for(nth_u32 i = 1; i < tmp_count; ++i) {
        nth_i32 key_prio = queue_priority(tmp[i].flags, tmp[i].supports_present);
        nth_i32 j = (nth_i32)i - 1;
        while(j >= 0 && queue_priority(tmp[j].flags, tmp[j].supports_present) > key_prio) {
            tmp[j + 1] = tmp[j];
            --j;
        }
        tmp[j + 1] = tmp[i];
    }

    static const nth_f32 priority = 1.0f;
    VkDeviceQueueCreateInfo queue_infos[NTH_VULKAN_MAX_QUEUES];
    for(nth_u32 i = 0; i < tmp_count; ++i) {
        queue_infos[i] = (VkDeviceQueueCreateInfo){
            .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = tmp[i].family,
            .queueCount       = 1,
            .pQueuePriorities = &priority
        };
    }

    nth_u32 total_ext_count = desc->extension_count;
    nth_b8 needs_swapchain = desc->surface != VK_NULL_HANDLE;
    if(needs_swapchain) ++total_ext_count;

    const char **all_extensions = NULL;
    if(total_ext_count > 0) {
        all_extensions = malloc(total_ext_count * sizeof(const char *));
        if(!all_extensions)
            return NTH_RESULT_OUT_OF_MEMORY;

        nth_u32 i = 0;
        for(; i < desc->extension_count; ++i)
            all_extensions[i] = desc->extensions[i];

        if(needs_swapchain)
            all_extensions[i] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    }

    VkDeviceCreateInfo device_info = {
        .sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext                   = desc->features,  /* VkPhysicalDeviceFeatures2 chain, or NULL */
        .queueCreateInfoCount    = tmp_count,
        .pQueueCreateInfos       = queue_infos,
        .enabledExtensionCount   = total_ext_count,
        .ppEnabledExtensionNames = all_extensions,
        .pEnabledFeatures        = NULL  /* must be NULL when using pNext features chain */
    };

    VkResult res = vkCreateDevice(best_device, &device_info, NULL, &device->device);
    free(all_extensions);

    if(res != VK_SUCCESS) {
        NTH_LOG_FATAL("Failed to create logical device");
        return NTH_RESULT_FAILED;
    }

    device->queue_count = tmp_count;
    for(nth_u32 i = 0; i < tmp_count; ++i) {
        device->queues[i] = tmp[i];
        vkGetDeviceQueue(device->device, tmp[i].family, 0, &device->queues[i].queue);

    }

    NTH_LOG_INFO("Vulkan device created");
    return NTH_RESULT_OK;
}
NTH_API void nth_teardown_vulkan_device(NthVulkanDevice *device) {
    if(device->device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device->device);
        vkDestroyDevice(device->device, NULL);
        device->device = VK_NULL_HANDLE;
    }

    g_vulkan.phy_devices[g_vulkan.phy_device_count++] = device->phy_device;
    device->phy_device = VK_NULL_HANDLE;

    device->queue_count = 0;
}
