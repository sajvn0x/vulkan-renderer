#pragma once

#include "renderer/driver_device.hh"

class DefaultGraphicsPipeline {
    struct Vertex {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec2 uv;
    };

    static_assert(offsetof(Vertex, position) == 0);
    static_assert(offsetof(Vertex, normal) == 12);
    static_assert(offsetof(Vertex, uv) == 24);
    static_assert(sizeof(Vertex) == 32);

    const std::array<Vertex, 3> vertices = {{
        {{-0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
        {{0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
        {{0.0f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.5f, 1.0f}},
    }};

    const std::array<uint32_t, 3> indices = {0, 1, 2};

    RenderingDeviceDriver* device_driver = nullptr;

    PipelineID pipeline;

    BufferID vertex_buffer;
    BufferID index_buffer;

    const char* program_name = "default";
    Ref<ShaderProgram> shader_program = nullptr;

public:
    DefaultGraphicsPipeline(RenderingDeviceDriver* p_device_driver, VkRenderPass p_render_pass);
    ~DefaultGraphicsPipeline();

    void draw(CommandBufferID p_cmd_buffer);
};
