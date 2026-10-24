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

    Ref<RenderPipeline> pipeline = nullptr;

    Ref<Buffer> vertex_buffer = nullptr;
    Ref<Buffer> index_buffer = nullptr;

public:
    DefaultGraphicsPipeline(RenderingDeviceDriver* p_device_driver, const char* p_program_name,
                            VkRenderPass p_render_pass);
    ~DefaultGraphicsPipeline();

    void ready_frame(Ref<CommandBuffer> p_cmd_buffer);
    void draw(Ref<CommandBuffer> p_cmd_buffer);
};
