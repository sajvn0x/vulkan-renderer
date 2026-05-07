#include "rendering_device.hh"

#include "core/error/error_macros.hh"

Error RenderingDevice::initialize() {
    driver_context = new RenderingDriverContext();
    Error err = driver_context->initialize();
    ERR_FAIL_COND_V(err != OK, err);

    driver_device = new RenderingDeviceDriver(driver_context);
    err = driver_device->initialize(0, 1);

    return OK;
}

RenderingDevice::RenderingDevice() {}

RenderingDevice::~RenderingDevice() {}
