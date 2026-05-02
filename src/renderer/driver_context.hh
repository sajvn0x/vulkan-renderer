#pragma once

#include <volk.h>

#include <cstdint>

#include "core/containers.hh"
#include "core/error/error_list.hh"

struct Vendor {
    constexpr static uint32_t VENDOR_UNKNOWN = 0x0;
    constexpr static uint32_t VENDOR_AMD = 0x1002;
    constexpr static uint32_t VENDOR_IMGTEC = 0x1010;
    constexpr static uint32_t VENDOR_APPLE = 0x106B;
    constexpr static uint32_t VENDOR_NVIDIA = 0x10DE;
    constexpr static uint32_t VENDOR_ARM = 0x13B5;
    constexpr static uint32_t VENDOR_MICROSOFT = 0x1414;
    constexpr static uint32_t VENDOR_QUALCOMM = 0x5143;
    constexpr static uint32_t VENDOR_INTEL = 0x8086;
};

enum DeviceType {
    DEVICE_TYPE_OTHER = 0x0,
    DEVICE_TYPE_INTEGRATED_GPU = 0x1,
    DEVICE_TYPE_DISCRETE_GPU = 0x2,
    DEVICE_TYPE_VIRTUAL_GPU = 0x3,
    DEVICE_TYPE_CPU = 0x4,
    DEVICE_TYPE_MAX = 0x5
};

struct Device {
    String name = "Unknown";
    uint32_t vendor = Vendor::VENDOR_UNKNOWN;
    DeviceType type = DEVICE_TYPE_OTHER;
};

class RenderingDriverContext {
   private:
    VkInstance instance = VK_NULL_HANDLE;
    uint32_t instance_api_version = VK_API_VERSION_1_0;
    HashMap<String, bool> requested_instance_extensions;
    HashSet<String> enabled_instance_extension_names;
    Vector<Device> driver_devices;

    Error _initialize_vulkan_version();
    Error _initialize_instance_extensions();
    Error _initialize_instance();
    Error _register_requested_instance_extension(const String& p_extension_name,
                                                 bool p_required);
    Error _initialize_devices();

   public:
    Error initialize();
    const Device& device_get(uint32_t p_device_index) const {
        return driver_devices[p_device_index];
    }
    uint32_t device_get_count() const { return driver_devices.size(); }

    RenderingDriverContext();
    ~RenderingDriverContext();
};
