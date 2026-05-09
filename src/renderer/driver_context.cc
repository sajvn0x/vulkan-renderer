#include "driver_context.hh"

#include <GLFW/glfw3.h>

#include "core/containers.hh"
#include "core/error/error_macros.hh"
#include "version.hh"

Error RenderingDriverContext::initialize() {
    Error err = OK;

    ERR_FAIL_COND_V_MSG(volkInitialize() != VK_SUCCESS, FAILED, "Failed to load volk");

    err = _initialize_vulkan_version();
    ERR_FAIL_COND_V(err != OK, err);

    err = _initialize_instance_extensions();
    ERR_FAIL_COND_V(err != OK, err);

    err = _initialize_instance();
    ERR_FAIL_COND_V(err != OK, err);

    volkLoadInstance(instance);

    err = _initialize_devices();
    ERR_FAIL_COND_V(err != OK, err);

    return err;
}

Error RenderingDriverContext::_initialize_vulkan_version() {
    VkResult result = vkEnumerateInstanceVersion(&instance_api_version.api_version);
    ERR_FAIL_COND_V(result != VK_SUCCESS, ERR_CANT_CREATE);
    ERR_FAIL_COND_V_MSG(!instance_api_version.supports_vulkan_1_2(), ERR_UNAVAILABLE,
                        "Vulkan driver not supported. Vulkan 1.2 or higher is required");

    return OK;
}

Error RenderingDriverContext::_initialize_instance_extensions() {
    enabled_instance_extension_names.clear();

    _register_requested_instance_extension(VK_KHR_SURFACE_EXTENSION_NAME, true);

    // glfw required instance extensions
    uint32_t glfw_required_extension_count = 0;
    const char** glfw_required_extensions =
        glfwGetRequiredInstanceExtensions(&glfw_required_extension_count);

    for (uint32_t i = 0; i < glfw_required_extension_count; ++i) {
        _register_requested_instance_extension(glfw_required_extensions[i], true);
    }

    _register_requested_instance_extension(VK_EXT_DEBUG_REPORT_EXTENSION_NAME, false);
    _register_requested_instance_extension(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
                                           false);
    _register_requested_instance_extension(VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME, false);
    _register_requested_instance_extension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME, false);

    uint32_t instance_extension_count = 0;
    VkResult err =
        vkEnumerateInstanceExtensionProperties(nullptr, &instance_extension_count, nullptr);
    ERR_FAIL_COND_V(err != VK_SUCCESS && err != VK_INCOMPLETE, ERR_CANT_CREATE);
    ERR_FAIL_COND_V_MSG(instance_extension_count == 0, ERR_CANT_CREATE,
                        "No instance extensions were found.");

    Vector<VkExtensionProperties> instance_extensions(instance_extension_count);
    err = vkEnumerateInstanceExtensionProperties(nullptr, &instance_extension_count,
                                                 instance_extensions.data());
    if (err != VK_SUCCESS && err != VK_INCOMPLETE) {
        ERR_FAIL_V(ERR_CANT_CREATE);
    }

    for (uint32_t i = 0; i < instance_extension_count; i++) {
        std::string extension_name = instance_extensions[i].extensionName;

        if (requested_instance_extensions.find(extension_name) !=
            requested_instance_extensions.end()) {
            enabled_instance_extension_names.insert(extension_name);
        }
    }

    for (auto& requested_extension : requested_instance_extensions) {
        const String& name = requested_extension.first;
        bool required = requested_extension.second;

        if (enabled_instance_extension_names.find(name) == enabled_instance_extension_names.end()) {
            if (required) {
                ERR_FAIL_V_MSG(ERR_CANT_CREATE,
                               ("Required extension " + name + " not found.").c_str());
            }
        }
    }

    return OK;
}

Error RenderingDriverContext::_initialize_instance() {
    Vector<const char*> enabled_extension_names;

    uint32_t application_api_version =
        instance_api_version.supports_vulkan_1_3() ? VK_API_VERSION_1_3 : VK_API_VERSION_1_2;

    VkApplicationInfo app_info = {};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = APP_NAME;
    app_info.pEngineName = APP_ENGINE_NAME;
    app_info.engineVersion =
        VK_MAKE_VERSION(APP_VERSION_MAJOR, APP_VERSION_MINOR, APP_VERSION_PATCH);
    app_info.apiVersion = application_api_version;

    Vector<const char*> enabled_layer_names;
    enabled_layer_names.push_back("VK_LAYER_KHRONOS_validation");

    VkInstanceCreateInfo instance_info = {};
    instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_info.enabledExtensionCount = enabled_extension_names.size();
    instance_info.ppEnabledExtensionNames = enabled_extension_names.data();
    instance_info.enabledLayerCount = enabled_layer_names.size();
    instance_info.ppEnabledLayerNames = enabled_layer_names.data();

    VkResult result = vkCreateInstance(&instance_info, nullptr, &instance);

    ERR_FAIL_COND_V_MSG(result == VK_ERROR_INCOMPATIBLE_DRIVER, ERR_UNAVAILABLE,
                        "Unsupported Vulkan driver.");
    ERR_FAIL_COND_V_MSG(result == VK_ERROR_EXTENSION_NOT_PRESENT, ERR_CANT_CREATE,
                        "Required Vulkan extensions are not available.");
    ERR_FAIL_COND_V_MSG(result != VK_SUCCESS, ERR_CANT_CREATE, "Failed to create Vulkan instance.");

    return OK;
}

void RenderingDriverContext::_register_requested_instance_extension(const String& p_extension_name,
                                                                    bool p_required) {
    requested_instance_extensions[p_extension_name] = p_required;
}

Error RenderingDriverContext::_initialize_devices() {
    uint32_t physical_device_count = 0;
    VkResult err = vkEnumeratePhysicalDevices(instance, &physical_device_count, nullptr);
    ERR_FAIL_COND_V(err != VK_SUCCESS, ERR_CANT_CREATE);
    ERR_FAIL_COND_V_MSG(physical_device_count == 0, ERR_CANT_CREATE,
                        "No Vulkan-compatible physical devices found");

    driver_devices.resize(physical_device_count);
    physical_devices.resize(physical_device_count);
    device_queue_families.resize(physical_device_count);
    err = vkEnumeratePhysicalDevices(instance, &physical_device_count, physical_devices.data());
    ERR_FAIL_COND_V(err != VK_SUCCESS, ERR_CANT_CREATE);

    // Fill the list of driver devices with the properties from the physical
    // devices.
    for (uint32_t i = 0; i < physical_devices.size(); i++) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(physical_devices[i], &props);

        Device& driver_device = driver_devices[i];
        driver_device.name = props.deviceName;
        driver_device.vendor = props.vendorID;
        driver_device.type = DeviceType(props.deviceType);

        uint32_t queue_family_properties_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physical_devices[i],
                                                 &queue_family_properties_count, nullptr);

        if (queue_family_properties_count > 0) {
            device_queue_families[i].properties.resize(queue_family_properties_count);
            vkGetPhysicalDeviceQueueFamilyProperties(physical_devices[i],
                                                     &queue_family_properties_count,
                                                     device_queue_families[i].properties.data());
        }
    }

    return OK;
}

const Device& RenderingDriverContext::device_get(uint32_t p_device_index) {
    DEV_ASSERT(p_device_index < driver_devices.size());
    return driver_devices[p_device_index];
}

uint32_t RenderingDriverContext::device_get_count() const { return driver_devices.size(); }

VkInstance RenderingDriverContext::instance_get() const { return instance; }

VkPhysicalDevice RenderingDriverContext::physical_device_get(uint32_t p_device_index) const {
    DEV_ASSERT(p_device_index < physical_devices.size());
    return physical_devices[p_device_index];
}

uint32_t RenderingDriverContext::queue_family_get_count(uint32_t p_device_index) const {
    DEV_ASSERT(p_device_index < physical_devices.size());
    return device_queue_families[p_device_index].properties.size();
}

VkQueueFamilyProperties RenderingDriverContext::queue_family_get(
    uint32_t p_device_index, uint32_t p_queue_family_index) const {
    DEV_ASSERT(p_device_index < physical_devices.size());
    DEV_ASSERT(p_queue_family_index < queue_family_get_count(p_device_index));
    return device_queue_families[p_device_index].properties[p_queue_family_index];
}

RenderingDriverContext::RenderingDriverContext() {}

RenderingDriverContext::~RenderingDriverContext() {}
