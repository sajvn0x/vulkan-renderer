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

    driver_device->swapchain_create(surface);

    return OK;
}

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

void Renderer::begin_frame() {}

void Renderer::next_frame() {}

Renderer::Renderer() {}

Renderer::~Renderer() {}
