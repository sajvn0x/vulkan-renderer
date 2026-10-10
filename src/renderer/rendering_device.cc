#include "rendering_device.hh"

#include "core/error/error_macros.hh"

Error RenderingDevice::initialize(GLFWwindow* window) {
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

    // create default graphics pipeline
    // simple snippet for creating a graphics pipeline
    // PipelineRasterizationState rasterization_state = {};
    // PipelineMultisampleState multisample_state = {};
    // PipelineDepthStencilState depth_stencil_state = {};
    // PipelineColorBlendState blend_state = {};
    // Vector<int32_t> color_attachments;
    // Vector<VkDynamicState> dynamic_states;
    //
    // Vector<Attachment> attachments;
    // Vector<Subpass> subpasses;
    // Vector<SubpassDependency> subpass_dependencies;
    // Ref<RenderPass> render_pass =
    //     driver_device->render_pass_create(attachments, subpasses, subpass_dependencies);
    //
    // driver_device->render_pipeline_create(
    //     "default", VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, rasterization_state, multisample_state,
    //     depth_stencil_state, blend_state, color_attachments, dynamic_states, render_pass, 1);

    return OK;
}

uint32_t RenderingDevice::_choose_physical_device() {
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

RenderingDevice::RenderingDevice() {}

RenderingDevice::~RenderingDevice() {
    // command pool
    // driver_device->command_pool_free(vk_primary_command_pool);
}
