#include "squares_bg.hh"

SquaresBGPipeline::SquaresBGPipeline(RenderingDeviceDriver* p_device_driver,
                                     VkRenderPass p_render_pass) {
    device_driver = p_device_driver;

    VkPrimitiveTopology primitive = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    PipelineRasterizationState rasterization = {};
    PipelineMultisampleState multisample = {};
    PipelineDepthStencilState depth_stencil = {};
    PipelineColorBlendState color_blend = PipelineColorBlendState::create_disabled();
    Vector<VkDynamicState> dynamic_states = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

    VkPipelineVertexInputStateCreateInfo vertex_input = {};
    vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input.vertexBindingDescriptionCount = 0;
    vertex_input.pVertexBindingDescriptions = nullptr;

    vertex_input.vertexAttributeDescriptionCount = 0;
    vertex_input.pVertexAttributeDescriptions = nullptr;

    pipeline = device_driver->render_pipeline_create(
        program_name, primitive, vertex_input, rasterization, multisample, depth_stencil,
        color_blend, {0}, dynamic_states, p_render_pass, 0);

    uniforms.clear();
    uniforms.resize(1);

    buffer = device_driver->buffer_create(
        sizeof(SceneData),
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        RenderingDeviceDriver::MEMORY_ALLOCATION_TYPE_GPU_MAPPABLE);

    Vector<BoundUniform> bound_uniforms;
    bound_uniforms.clear();
    bound_uniforms.resize(1);

    bound_uniforms[0].buffers.clear();
    bound_uniforms[0].buffers.resize(1);
    bound_uniforms[0].buffers[0] = buffer;

    bound_uniforms[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bound_uniforms[0].binding = 0;

    uniforms[0] = device_driver->uniform_set_create(bound_uniforms, pipeline->shader_program, 0);
}

SquaresBGPipeline::~SquaresBGPipeline() { device_driver = nullptr; }

void SquaresBGPipeline::draw(Ref<CommandBuffer> p_cmd_buffer) {
    device_driver->command_bind_render_pipeline(p_cmd_buffer, pipeline);

    Vector<VkRect2D> viewports;
    viewports.clear();
    viewports.resize(1);
    viewports[0].extent = {.width = 1920, .height = 1080};
    viewports[0].offset = {.x = 0, .y = 0};

    Vector<VkRect2D> scissors;
    scissors.clear();
    scissors.resize(1);
    scissors[0].extent = {.width = 1920, .height = 1080};
    scissors[0].offset = {.x = 0, .y = 0};

    device_driver->command_bind_render_uniform_sets(p_cmd_buffer, uniforms,
                                                    pipeline->shader_program, 0, 1);

    device_driver->command_render_set_viewport(p_cmd_buffer, viewports);
    device_driver->command_render_set_scissor(p_cmd_buffer, scissors);
    device_driver->command_render_draw(p_cmd_buffer, 3, 1, 0, 0);
}

void SquaresBGPipeline::update_scene_data(const SceneData& p_scene_data) {
    void* mapped = device_driver->buffer_map(buffer);
    memcpy(mapped, &p_scene_data, sizeof(SceneData));
    device_driver->buffer_unmap(buffer);
}
