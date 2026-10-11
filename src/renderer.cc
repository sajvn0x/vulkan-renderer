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
    // TODO: replace it with a VALID graphics queue index
    primary_command_pool = driver_device->command_pool_create(0, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    ERR_FAIL_COND_V_MSG(primary_command_pool == nullptr, ERR_CANT_CREATE,
                        "Failed to create primary command pool");

    // command queue
    command_queue = driver_device->command_queue_create(0);
    in_flight_fence = driver_device->fence_create();

    uint32_t swapchain_image_count = swapchain->images.size();
    cmd_buffers.resize(swapchain_image_count);
    for (uint32_t i = 0; i < swapchain_image_count; ++i) {
        cmd_buffers.push_back(driver_device->command_buffer_create(primary_command_pool));
    }

    return OK;
}

void Renderer::begin_frame() {
    // driver_device->fence_wait(in_flight_fence);
    // // acquire the swapchain images
    // driver_device->swapchain_acquire_next_image(swapchain,
    // command_queue->image_available_semaphore,
    //                                             in_flight_fence);
    // uint32_t image_index = swapchain->image_index;
    //
    //    Ref<CommandBuffer> cmd_buffer = cmd_buffers[image_index];
    //    driver_device->command_buffer_begin(cmd_buffer);
    //    driver_device->command_buffer_end(cmd_buffer);
    //
    //    driver_device->command_queue_execute_and_present(command_queue, in_flight_fence,
    //    swapchain,
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
