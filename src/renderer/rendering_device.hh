#pragma once

#include "driver_context.hh"
#include "driver_device.hh"

class RenderingDevice {
    RenderingDriverContext* driver_context = nullptr;
    RenderingDeviceDriver* driver_device = nullptr;

   private:
    uint32_t _choose_physical_device();

   public:
    Error initialize();

    RenderingDevice();
    ~RenderingDevice();
};
