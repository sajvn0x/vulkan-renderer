#pragma once

#include <spirv_reflect.h>
#include <vulkan/vulkan.h>

#include "core/containers.hh"
#include "core/error/error_list.hh"
#include "core/traits.hh"

enum PipelineType {
    PIPELINE_TYPE_GRAPHICS,
    PIPELINE_TYPE_COMPUTE,
};

struct DescriptorBinding {
    VkDescriptorSetLayoutBinding layout;
    String name;
    uint32_t size = 0;
};

struct DescriptorSet {
    uint32_t set = 0;
    Vector<DescriptorBinding> bindings;
};

struct PushConstantRange {
    VkPushConstantRange vk_range = {};
    String name;
};

struct VertexInputLayout {
    Vector<VkVertexInputBindingDescription> bindings;
    Vector<VkVertexInputAttributeDescription> attributes;
    VkPipelineVertexInputStateCreateInfo vk_state = {};
};

struct SpecializationConstant {
    VkSpecializationMapEntry vk_entry = {};
    String name;
};

struct SpecializationInfo {
    Vector<SpecializationConstant> constants;
    VkSpecializationInfo vk_info = {};
};

struct ShaderReflection {
    VkShaderStageFlags stage;
    uint32_t* source;
    uint32_t source_size = 0;
    String source_path;
    String entry_point = "main";
    Vector<DescriptorSet> descriptor_sets;
    Vector<PushConstantRange> push_constants;
    Vector<SpecializationConstant> specialization_constants;
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

struct ShaderProgram : RefTarget<ShaderProgram> {
    PipelineType pipeline_type = PIPELINE_TYPE_GRAPHICS;
    Vector<Ref<ShaderModuleAsset>> shaders;

    bool has_vertex_shader = false;
    bool has_fragment_shader = false;
    bool has_geometry_shader = false;
    bool has_tese_shader = false;
    bool has_tesc_shader = false;
    bool has_compute_shader = false;

    VertexInputLayout vk_vertex_info = {};
    VkPipelineLayout vk_pipeline_layout = VK_NULL_HANDLE;
    Vector<VkDescriptorSetLayout> descriptor_set_layouts;
    Vector<VkShaderModule> vk_shader_modules;
    Vector<VkPipelineShaderStageCreateInfo> vk_pipeline_stages;

    /*
        Capable of finding the related shaders with shaders have the same name
    */
    Error load_shader_program(const String& shader_name, VkDevice vk_device);

   private:
    Error _build_shader_module_asset(const String& name);
    ShaderReflection _shader_reflection(const String& name);
    void _build_vertex_info(Vector<SpvReflectInterfaceVariable*>& variables);
    void _build_pipeline_layout(VkDevice vk_device);
    void _build_shader_module(VkDevice vk_device);
};
