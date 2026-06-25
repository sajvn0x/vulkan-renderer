#pragma once

#include "display.hh"
#include "driver_context.hh"
#include "driver_device.hh"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

class RenderingDevice {
    RenderingDriverContext* driver_context = nullptr;
    RenderingDeviceDriver* driver_device = nullptr;

    Surface surface;

    Ref<CommandPool> vk_primary_command_pool = nullptr;

   private:
    uint32_t _choose_physical_device();

   public:
    Error initialize(GLFWwindow* window);

    RenderingDevice();
    ~RenderingDevice();
};
