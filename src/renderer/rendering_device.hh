#pragma once

#include "driver_context.hh"
#include "driver_device.hh"

class RenderingDevice {
    RenderingDriverContext* driver_context = nullptr;
    RenderingDeviceDriver* driver_device = nullptr;

   public:
    Error initialize();

    RenderingDevice();
    ~RenderingDevice();
};
