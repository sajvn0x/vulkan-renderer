#include "renderer.hh"

#include "core/error/error_macros.hh"

Error Renderer::initialize(GLFWwindow* window) {
    driver_context = new RenderingDriverContext();
    Error err = driver_context->initialize();
    ERR_FAIL_COND_V(err != OK, err);

    // surface
    VkResult result = glfwCreateWindowSurface(driver_context->instance_get(), window, nullptr,
                                              &surface.vk_surface);
    ERR_FAIL_COND_V_MSG(result != VK_SUCCESS, ERR_CANT_CREATE, "Failed to create surface");

    int width, height;
    glfwGetWindowSize(window, &width, &height);
    surface.width = width;
    surface.height = height;

    driver_device = new RenderingDeviceDriver(driver_context);

    uint32_t physical_device_index = _choose_physical_device();
    uint32_t frame_count = 1;

    err = driver_device->initialize(physical_device_index, frame_count);
    ERR_FAIL_COND_V(err != OK, err);

    // swapchain
    swapchain = driver_device->swapchain_create(surface);
    ERR_FAIL_COND_V_MSG(err != OK, ERR_CANT_CREATE, "Failed to create swapchain");

    // command pool
    primary_command_pool = driver_device->command_pool_create(0, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    ERR_FAIL_COND_V_MSG(primary_command_pool == nullptr, ERR_CANT_CREATE,
                        "Failed to create primary command pool");

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        Ref<Fence> fence = driver_device->fence_create();
        ERR_FAIL_COND_V_MSG(fence == nullptr, ERR_CANT_CREATE, "Couldn't able to create fence");
        in_flight_fences.push_back(fence);
    }

    uint32_t swapchain_image_count = swapchain->images.size();
    cmd_buffers.resize(swapchain_image_count);
    for (uint32_t i = 0; i < swapchain_image_count; ++i) {
        cmd_buffers[i] = driver_device->command_buffer_create(primary_command_pool);
    }

    // command queue
    command_queue = driver_device->command_queue_create(0, swapchain_image_count);

    default_pipeline =
        new DefaultGraphicsPipeline(driver_device, "default", swapchain->vk_render_pass);

    return OK;
}

void Renderer::begin_frame() {
    driver_device->fence_wait(in_flight_fences[current_frame]);
    driver_device->swapchain_acquire_next_image(swapchain,
                                                command_queue->acquire_semaphores[current_frame],
                                                in_flight_fences[current_frame]);

    uint32_t image_index = swapchain->image_index;

    Ref<CommandBuffer> cmd_buffer = cmd_buffers[image_index];
    driver_device->command_buffer_begin(cmd_buffer);

    Vector<VkClearValue> clear_values;
    clear_values.push_back(VkClearValue{.color = {{0.0f, 0.0f, 0.0f, 1.0f}}});
    VkRect2D render_area = {
        .offset = {.x = 0, .y = 0},
        .extent = {.width = 1920, .height = 1011},
    };
    driver_device->command_begin_render_pass(
        cmd_buffer, swapchain->vk_render_pass, swapchain->framebuffers[image_index],
        VK_COMMAND_BUFFER_LEVEL_PRIMARY, render_area, clear_values);

    // render a triangle using default pipeline
    default_pipeline->draw(cmd_buffer);

    driver_device->command_end_render_pass(cmd_buffer);

    driver_device->command_buffer_end(cmd_buffer);

    driver_device->command_queue_execute_and_present(command_queue, in_flight_fences[current_frame],
                                                     swapchain, cmd_buffer, current_frame);

    current_frame = (current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void Renderer::next_frame() {}

uint32_t Renderer::_choose_physical_device() {
    HashMap<uint32_t, uint32_t> scores;

    for (uint32_t i = 0; i < driver_context->device_get_count(); ++i) {
        VkPhysicalDeviceProperties2 physical_device_props = {};
        physical_device_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;

        vkGetPhysicalDeviceProperties2(driver_context->physical_device_get(i),
                                       &physical_device_props);
        uint32_t score = 0;

        switch (physical_device_props.properties.deviceType) {
            case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
                score += 5;
                break;
            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
                score += 4;
                break;
            default:
                score += 3;
                break;
        }

        scores.insert({i, score});
    }

    uint32_t best_index = 0;
    uint32_t best_score = 0;

    for (const auto& [index, score] : scores) {
        if (score > best_score) {
            best_score = score;
            best_index = index;
        }
    }

    return best_index;
}

Renderer::Renderer() {}

Renderer::~Renderer() {
    if (swapchain) driver_device->swapchain_free(swapchain);
}
