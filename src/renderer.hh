#pragma once

#include "core/error/error_list.hh"
#include "renderer/driver_context.hh"
#include "renderer/driver_device.hh"
#include "renderer/types.hh"
#include "shaders/default.hh"
#include "shaders/squares_bg.hh"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

class Renderer {
    RenderingDriverContext* driver_context = nullptr;
    RenderingDeviceDriver* driver_device = nullptr;
    Surface surface;
    Ref<Swapchain> swapchain = nullptr;
    Ref<CommandPool> primary_command_pool = nullptr;

    Ref<CommandQueue> command_queue = nullptr;
    Vector<Ref<Fence>> in_flight_fences;
    Vector<Ref<CommandBuffer>> cmd_buffers;
    uint32_t current_frame = 0;

    Vector<Ref<Fence>> fences;

    // pipeline
    DefaultGraphicsPipeline* default_pipeline = nullptr;
    SquaresBGPipeline* squares_bg_pipeline = nullptr;

private:
    uint32_t _choose_physical_device();

public:
    Renderer();
    ~Renderer();

    Error initialize(GLFWwindow* window);
    void begin_frame();
    void next_frame();
};
