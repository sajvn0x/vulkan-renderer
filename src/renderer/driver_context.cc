#include "driver_context.hh"

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

    return err;
}

Error RenderingDriverContext::_initialize_instance_extensions() {
    Error err = OK;

    return err;
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

RenderingDriverContext::RenderingDriverContext() {}

RenderingDriverContext::~RenderingDriverContext() {}
