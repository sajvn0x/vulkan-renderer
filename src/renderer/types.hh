#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>

#include "core/containers.hh"

struct Size2i {
    float x, y;
};

/* capabilities */
struct SubgroupCapabilities {
    uint32_t size = 0;
    uint32_t min_size = 0;
    uint32_t max_size = 0;
    VkShaderStageFlags supported_stages = 0;

    VkSubgroupFeatureFlags supported_operations = 0;
    VkBool32 quad_operations_in_all_stages = false;
    bool size_control_is_supported = false;

    uint32_t supported_stages_flags_rd() const;
    String supported_stages_desc() const;
    uint32_t supported_operations_flags_rd() const;
    String supported_operations_desc() const;
};

struct ShaderCapabilities {
    bool shader_float16_is_supported = false;
    bool shader_int8_is_supported = false;
};

struct StorageBufferCapabilities {
    bool storage_buffer_16_bit_access_is_supported = false;
    bool uniform_and_storage_buffer_16_bit_access_is_supported = false;
    bool storage_push_constant_16_is_supported = false;
    bool storage_input_output_16 = false;
};

struct MultiviewCapabilities {
    bool is_supported = false;
    bool geometry_shader_is_supported = false;
    bool tessellation_shader_is_supported = false;
    uint32_t max_view_count = 0;
    uint32_t max_instance_count = 0;
};

struct FragmentShadingRateCapabilities {
    Size2i min_texel_size;
    Size2i max_texel_size;
    Size2i max_fragment_size;
    bool pipeline_supported = false;
    bool primitive_supported = false;
    bool attachment_supported = false;
};

struct FragmentDensityMapCapabilities {
    Size2i min_texel_size;
    Size2i max_texel_size;
    Size2i offset_granularity;
    bool attachment_supported = false;
    bool dynamic_attachment_supported = false;
    bool non_subsampled_images_supported = false;
    bool invocations_supported = false;
    bool offset_supported = false;
};
