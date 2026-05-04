#include "driver_context.hh"

#include "core/containers.hh"
#include "core/error/error_macros.hh"

Error RenderingDriverContext::initialize() {
    Error err = OK;

    if (volkInitialize() != VK_SUCCESS) {
        return FAILED;
    }

    err = _initialize_vulkan_version();
    ERR_FAIL_COND_V(err != OK, err);

    err = _initialize_instance_extensions();
    ERR_FAIL_COND_V(err != OK, err);

    err = _initialize_instance();
    ERR_FAIL_COND_V(err != OK, err);

    err = _initialize_devices();
    ERR_FAIL_COND_V(err != OK, err);

    return err;
}

Error RenderingDriverContext::_initialize_vulkan_version() {
    Error err = OK;
    vkEnumerateInstanceVersion(&instance_api_version);

    return err;
}

Error RenderingDriverContext::_initialize_instance_extensions() {
    enabled_instance_extension_names.clear();

    _register_requested_instance_extension(VK_KHR_SURFACE_EXTENSION_NAME, true);
    if (_get_platform_surface_extension()) {
        _register_requested_instance_extension(
            _get_platform_surface_extension(), true);
    }
    _register_requested_instance_extension(VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
                                           false);
    _register_requested_instance_extension(
        VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME, false);
    _register_requested_instance_extension(
        VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME, false);

    _register_requested_instance_extension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
                                           false);

    uint32_t instance_extension_count = 0;
    VkResult err = vkEnumerateInstanceExtensionProperties(
        nullptr, &instance_extension_count, nullptr);
    ERR_FAIL_COND_V(err != VK_SUCCESS && err != VK_INCOMPLETE, ERR_CANT_CREATE);
    ERR_FAIL_COND_V_MSG(instance_extension_count == 0, ERR_CANT_CREATE,
                        "No instance extensions were found.");

    Vector<VkExtensionProperties> instance_extensions(instance_extension_count);
    err = vkEnumerateInstanceExtensionProperties(
        nullptr, &instance_extension_count, instance_extensions.data());
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

        if (enabled_instance_extension_names.find(name) ==
            enabled_instance_extension_names.end()) {
            if (required) {
                ERR_FAIL_V_MSG(ERR_CANT_CREATE, String("Required extension ") +
                                                    name +
                                                    String(" not found."));
            } else {
            };
        }
    }

    return OK;
}

Error RenderingDriverContext::_initialize_instance() {
    Error err = OK;

    return err;
}

Error RenderingDriverContext::_register_requested_instance_extension(
    const String& p_extension_name, bool p_required) {
    Error err = OK;

    return err;
}

Error RenderingDriverContext::_initialize_devices() {
    Error err = OK;

    return err;
}

const char* RenderingDriverContext::_get_platform_surface_extension() {
    return "VK_KHR_xcb_surface";
}

RenderingDriverContext::RenderingDriverContext() {}

RenderingDriverContext::~RenderingDriverContext() {}
