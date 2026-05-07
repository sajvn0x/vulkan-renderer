#pragma once

#include <volk.h>

#include <cstdint>

#include "core/containers.hh"
#include "core/error/error_list.hh"
#include "types.hh"

class RenderingDriverContext {
   private:
    struct DeviceQueueFamilies {
        Vector<VkQueueFamilyProperties> properties;
    };

    VkInstance instance = VK_NULL_HANDLE;
    uint32_t instance_api_version = VK_API_VERSION_1_0;
    HashMap<String, bool> requested_instance_extensions;
    HashSet<String> enabled_instance_extension_names;
    Vector<Device> driver_devices;
    Vector<VkPhysicalDevice> physical_devices;
    Vector<DeviceQueueFamilies> device_queue_families;
    VkDebugUtilsMessengerEXT debug_messenger = VK_NULL_HANDLE;
    VkDebugReportCallbackEXT debug_report = VK_NULL_HANDLE;

    Error _initialize_vulkan_version();
    Error _initialize_instance_extensions();
    Error _initialize_instance();
    Error _register_requested_instance_extension(const String& p_extension_name, bool p_required);
    Error _initialize_devices();

   public:
    Error initialize();
    const Device& device_get(uint32_t p_device_index);
    uint32_t device_get_count() const;

    VkInstance instance_get() const;
    VkPhysicalDevice physical_device_get(uint32_t p_device_index) const;
    uint32_t queue_family_get_count(uint32_t p_device_index) const;
    VkQueueFamilyProperties queue_family_get(uint32_t p_device_index,
                                             uint32_t p_queue_family_index) const;

    RenderingDriverContext();
    ~RenderingDriverContext();
};
