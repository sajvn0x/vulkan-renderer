#include "default.hh"

#include "vulkan/vulkan_core.h"

DefaultGraphicsPipeline::DefaultGraphicsPipeline(RenderingDeviceDriver* p_device_driver,
                                                 VkRenderPass p_render_pass) {
    device_driver = p_device_driver;

    shader_program = new ShaderProgram();
    Error err = shader_program->load_shader_program(program_name, p_device_driver->get_device());
    if (err != OK || shader_program->pipeline_type != PIPELINE_TYPE_GRAPHICS) {
        return;
    }

    // _create_render_pass(p_format);

    VkPrimitiveTopology primitive = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    PipelineRasterizationState rasterization = {};
    rasterization.cull_mode = VK_CULL_MODE_BACK_BIT;
    rasterization.front_face = VK_FRONT_FACE_CLOCKWISE;
    PipelineMultisampleState multisample = {};
    PipelineDepthStencilState depth_stencil = {};
    PipelineColorBlendState color_blend = PipelineColorBlendState::create_disabled();
    Vector<VkDynamicState> dynamic_states = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

    VkVertexInputBindingDescription binding = {};
    binding.binding = 0;
    binding.stride = sizeof(Vertex), binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attributes[3] = {};

    attributes[0].location = 0;
    attributes[0].binding = 0;
    attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributes[0].offset = offsetof(Vertex, position);

    attributes[1].location = 1;
    attributes[1].binding = 0;
    attributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributes[1].offset = offsetof(Vertex, normal);

    attributes[2].location = 2;
    attributes[2].binding = 0;
    attributes[2].format = VK_FORMAT_R32G32_SFLOAT;
    attributes[2].offset = offsetof(Vertex, uv);

    VkPipelineVertexInputStateCreateInfo vertex_input = {};
    vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input.vertexBindingDescriptionCount = 1;
    vertex_input.pVertexBindingDescriptions = &binding;

    vertex_input.vertexAttributeDescriptionCount = 3;
    vertex_input.pVertexAttributeDescriptions = attributes;

    pipeline = device_driver->render_pipeline_create(
        shader_program, primitive, vertex_input, rasterization, multisample, depth_stencil,
        color_blend, {0}, dynamic_states, p_render_pass, 0);

    vertex_buffer =
        device_driver->buffer_create(sizeof(vertices), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                     RenderingDeviceDriver::MEMORY_ALLOCATION_TYPE_GPU_MAPPABLE);
    index_buffer =
        device_driver->buffer_create(sizeof(indices), VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                     RenderingDeviceDriver::MEMORY_ALLOCATION_TYPE_GPU_MAPPABLE);

    uint8_t* vertex_data = device_driver->buffer_map(vertex_buffer);
    memcpy(vertex_data, vertices.data(), sizeof(vertices));

    uint8_t* index_data = device_driver->buffer_map(index_buffer);
    memcpy(index_data, indices.data(), sizeof(indices));

    device_driver->buffer_unmap(vertex_buffer);
    device_driver->buffer_unmap(index_buffer);
}

DefaultGraphicsPipeline::~DefaultGraphicsPipeline() { device_driver = nullptr; }

void DefaultGraphicsPipeline::ready_frame(Ref<CommandBuffer> p_cmd_buffer) {
    // Ref<Buffer> transfer_buffer = device_driver->buffer_create(1000,
    // VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    // RenderingDeviceDriver::MEMORY_ALLOCATION_TYPE_GPU_MAPPABLE); uint8_t *data =
    // device_driver->buffer_map(transfer_buffer); memcpy(data, vertices.data(), (size_t)
    // sizeof(vertices)); device_driver->buffer_unmap(transfer_buffer);
}

void DefaultGraphicsPipeline::draw(Ref<CommandBuffer> p_cmd_buffer) {
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

    device_driver->command_render_set_viewport(p_cmd_buffer, viewports);
    device_driver->command_render_set_scissor(p_cmd_buffer, scissors);
    device_driver->command_render_bind_vertex_buffers(p_cmd_buffer, 1, vertex_buffer, 0);
    device_driver->command_render_bind_index_buffer(p_cmd_buffer, index_buffer,
                                                    VK_INDEX_TYPE_UINT32, 0);
    device_driver->command_render_draw_indexed(p_cmd_buffer, static_cast<uint32_t>(indices.size()),
                                               1, 0, 0, 0);
}

/*
_ALWAYS_INLINE_ void DefaultGraphicsPipeline::_create_render_pass(VkFormat p_format) {
    Vector<Attachment> attachments;
    attachments.clear();

    Attachment color = {};
    color.format = p_format;
    color.samples = VK_SAMPLE_COUNT_1_BIT;
    color.load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.store_op = VK_ATTACHMENT_STORE_OP_STORE;
    color.stencil_load_op = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.stencil_store_op = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initial_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    color.final_layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    attachments.push_back(color);

    Attachment depth = {};
    depth.format = VK_FORMAT_D32_SFLOAT;
    depth.samples = VK_SAMPLE_COUNT_1_BIT;
    depth.load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth.store_op = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth.stencil_load_op = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depth.stencil_store_op = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth.initial_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    depth.final_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    attachments.push_back(depth);

    Vector<Subpass> subpasses;
    subpasses.clear();

    Subpass subpass = {};
    subpass.color_references.clear();

    VkAttachmentReference2 color_ref = {};
    color_ref.sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2;
    color_ref.attachment = 0;
    color_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    color_ref.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subpass.color_references.push_back(color_ref);

    VkAttachmentReference2 depth_ref = {};
    depth_ref.sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2;
    depth_ref.attachment = 1;
    depth_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depth_ref.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    subpass.depth_stencil_reference = depth_ref;
    subpasses.push_back(subpass);

    Vector<SubpassDependency> dependencies;

    SubpassDependency dependency = {};

    dependency.src_subpass = VK_SUBPASS_EXTERNAL;
    dependency.dst_subpass = 0;

    dependency.src_stages =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dst_stages =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.src_access = 0;
    dependency.dst_access =
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies.push_back(dependency);

    render_pass = device_driver->render_pass_create(attachments, subpasses, dependencies);
}
*/
