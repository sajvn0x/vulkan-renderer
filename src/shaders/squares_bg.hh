#pragma once

#include "renderer/driver_device.hh"

class SquaresBGPipeline {
    RenderingDeviceDriver* device_driver = nullptr;
    const char* program_name = "squares_bg";

    PipelineID pipeline;
    BufferID buffer;
    Vector<UniformSetID> uniforms;

    Ref<ShaderProgram> shader_program = nullptr;

public:
    struct alignas(16) SceneData {
        float resolution[2];
        float time;
        float padding = 0.0f;
    };
    static_assert(sizeof(SceneData) == 16);

    SquaresBGPipeline(RenderingDeviceDriver* p_device_driver, VkRenderPass p_render_pass);
    ~SquaresBGPipeline();

    void draw(CommandBufferID p_cmd_buffer);
    void update_scene_data(const SceneData& p_scene_data);
};
