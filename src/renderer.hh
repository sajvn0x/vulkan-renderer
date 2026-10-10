#pragma once

#include "core/error/error_list.hh"
#include "renderer/driver_context.hh"
#include "renderer/driver_device.hh"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

class Renderer {
    RenderingDriverContext* driver_context = nullptr;
    RenderingDeviceDriver* driver_device = nullptr;
    Surface surface;

private:
    uint32_t _choose_physical_device();

public:
    Renderer();
    ~Renderer();

    Error initialize(GLFWwindow* window);
    void begin_frame();
    void next_frame();
};
