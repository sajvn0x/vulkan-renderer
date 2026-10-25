#pragma once

#include "renderer/driver_device.hh"

class SquaresBGPipeline {
    RenderingDeviceDriver* device_driver = nullptr;
    const char* program_name = "squares_bg";

    Ref<RenderPipeline> pipeline = nullptr;
    Ref<Buffer> buffer;
    Vector<Ref<UniformSet>> uniforms;

public:
    struct alignas(16) SceneData {
        float resolution[2];
        float time;
        float padding = 0.0f;
    };
    static_assert(sizeof(SceneData) == 16);

    SquaresBGPipeline(RenderingDeviceDriver* p_device_driver, VkRenderPass p_render_pass);
    ~SquaresBGPipeline();

    void draw(Ref<CommandBuffer> p_cmd_buffer);
    void update_scene_data(const SceneData& p_scene_data);
};
