#pragma once

#include <vulkan/vulkan.h>

#include "core/containers.hh"
#include "core/error/error_list.hh"
#include "types.hh"

enum PipelineType {
    PIPELINE_TYPE_GRAPHICS,
    PIPELINE_TYPE_COMPUTE,
};

struct DescriptorBinding {
	VkDescriptorSetLayoutBinding layout;
    String name;
    uint32_t size = 0;
    bool is_array = false;
};

struct DescriptorSet {
    uint32_t set = 0;
    Vector<DescriptorBinding> bindings;
};

struct PushConstantRange {
    uint32_t offset = 0;
    uint32_t size = 0;
    String name;
};

struct VertexInputLayout {
    uint32_t stride = 0;
    Vector<VertexAttribute> attributes;
};

enum class SpecConstantType { Int, Float, Bool };

struct SpecializationConstant {
    uint32_t id = 0;
    String name;
    SpecConstantType type;
    union {
        int i;
        float f;
        bool b;
    } default_value;
};

struct ShaderReflection {
    VkShaderStageFlags stage;

    String entry_point = "main";
    String source_path;

    // descriptor sets
    Vector<DescriptorSet> descriptor_sets;

    // push constants
    Vector<PushConstantRange> push_constants;

    // vertex input
    VertexInputLayout vertex_input;

    // specialization constants
    Vector<SpecializationConstant> specialization_constants;
};

struct VertexFormatInfo {
    Vector<VkVertexInputBindingDescription> vk_bindings;
    Vector<VkVertexInputAttributeDescription> vk_attributes;
    VkPipelineVertexInputStateCreateInfo vk_create_info = {};
};

struct ShaderModuleAsset : RefTarget<ShaderModuleAsset> {
    String source_path;
    ShaderReflection reflection;
    VkShaderModule vk_module = VK_NULL_HANDLE;
    uint64_t last_write_time = 0;
    bool dirty = false;
    Vector<String> dependencies;

    bool reflect();
    bool reload();
};

struct ShaderProgram {
    PipelineType pipeline_type = PIPELINE_TYPE_GRAPHICS;
    Vector<Ref<ShaderModuleAsset>> shaders;

    VkVertexInputAttributeDescription vk_vertex_info = {};

    /*
        Capable of finding the related shaders with shaders have the same name
    */
    Error load_shader_program(const String& shader_name);

   private:
    Error _build_shader_module_asset(const String& name);
    ShaderReflection* _shader_reflection(const String& name);
    Error _build_vertex_info();
};
