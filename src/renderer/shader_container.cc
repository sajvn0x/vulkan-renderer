#include "shader_container.hh"

#include <spirv_reflect.h>

#include <algorithm>

#include "core/error/error_macros.hh"
#include "core/fs/fs.hh"

const uint32_t MAX_SHADER_FILES_PER_PROGRAM = 5;

static size_t vk_format_size(VkFormat format) {
    switch (format) {
        case VK_FORMAT_R32G32B32A32_SFLOAT:
            return 4 * sizeof(float);
        case VK_FORMAT_R32G32B32_SFLOAT:
            return 3 * sizeof(float);
        case VK_FORMAT_R32G32_SFLOAT:
            return 2 * sizeof(float);
        case VK_FORMAT_R32_SFLOAT:
            return sizeof(float);
        case VK_FORMAT_R32G32B32A32_SINT:
            return 4 * sizeof(int32_t);
        case VK_FORMAT_R32G32B32_SINT:
            return 3 * sizeof(int32_t);
        case VK_FORMAT_R32G32_SINT:
            return 2 * sizeof(int32_t);
        case VK_FORMAT_R32_SINT:
            return sizeof(int32_t);
        case VK_FORMAT_R32G32B32A32_UINT:
            return 4 * sizeof(uint32_t);
        case VK_FORMAT_R32G32B32_UINT:
            return 3 * sizeof(uint32_t);
        case VK_FORMAT_R32G32_UINT:
            return 2 * sizeof(uint32_t);
        case VK_FORMAT_R32_UINT:
            return sizeof(uint32_t);
        default:
            return 0;
    }
}

Error ShaderProgram::load_shader_program(const String& shader_name, VkDevice vk_device) {
    Vector<Path> shader_files = FileSystem::find_shader_files_by_name(shader_name);
    ERR_FAIL_COND_V(shader_files.size() > MAX_SHADER_FILES_PER_PROGRAM, FAILED);
    Error err = OK;

    // detect the pipeline type
    if (shader_files.size() == 1) {
        pipeline_type = PIPELINE_TYPE_COMPUTE;
    } else if (shader_files.size() >= 2) {
        pipeline_type = PIPELINE_TYPE_GRAPHICS;
    };

    for (Path& shader : shader_files) {
        err = _build_shader_module_asset(shader);
        ERR_FAIL_COND_V(err != OK, err);
    }

    // pipeline validation
    if (pipeline_type == PIPELINE_TYPE_GRAPHICS) {
        ERR_FAIL_COND_V(!has_vertex_shader || !has_fragment_shader, err);
        ERR_FAIL_COND_V(err != OK, err);
    } else if (pipeline_type == PIPELINE_TYPE_COMPUTE) {
        ERR_FAIL_COND_V(!has_compute_shader, err);
    }

    _build_pipeline_layout(vk_device);
    _build_shader_module(vk_device);

    return err;
}

Error ShaderProgram::_build_shader_module_asset(const String& name) {
    Ref<ShaderModuleAsset> shader_module_asset = new ShaderModuleAsset();

    ShaderReflection reflection = _shader_reflection(name);

    shader_module_asset->source_path = name;
    shader_module_asset->reflection = reflection;

    shaders.push_back(shader_module_asset);

    return OK;
}

ShaderReflection ShaderProgram::_shader_reflection(const String& name) {
    ShaderReflection shader_reflection = {};

    SpirvFileResult file_result = FileSystem::read_spirv_file(name);
    ERR_FAIL_COND_V(!file_result.success, shader_reflection);

    // spirv source
    shader_reflection.source = file_result.content.data();
    shader_reflection.source_size = file_result.size;

    spv_reflect::ShaderModule shader_module =
        spv_reflect::ShaderModule(shader_reflection.source_size, shader_reflection.source);

    ERR_FAIL_COND_V(shader_module.GetResult() != SPV_REFLECT_RESULT_SUCCESS, shader_reflection);

    // entry point
    shader_reflection.entry_point = shader_module.GetEntryPointName();

    // shader stage
    VkShaderStageFlagBits stage = (VkShaderStageFlagBits)shader_module.GetShaderStage();
    ERR_FAIL_COND_V_MSG(stage > VK_SHADER_STAGE_COMPUTE_BIT, shader_reflection,
                        "For now, we only support primitive shaders, and compute shaders");
    shader_reflection.stage = stage;

    // input variables (vertex only)
    uint32_t input_variable_count = 0;
    shader_module.EnumerateInputVariables(&input_variable_count, nullptr);
    Vector<SpvReflectInterfaceVariable*> input_variables(input_variable_count);
    shader_module.EnumerateInputVariables(&input_variable_count, input_variables.data());

    switch (shader_reflection.stage) {
        case (VK_SHADER_STAGE_VERTEX_BIT):
            has_vertex_shader = true;
            _build_vertex_info(input_variables);
            break;
        case (VK_SHADER_STAGE_FRAGMENT_BIT):
            has_fragment_shader = true;
            break;
        case (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT):
            has_tesc_shader = true;
            break;
        case (VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT):
            has_tese_shader = true;
            break;
        case (VK_SHADER_STAGE_GEOMETRY_BIT):
            has_geometry_shader = true;
            break;
        case (VK_SHADER_STAGE_COMPUTE_BIT):
            has_compute_shader = true;
            break;
    }

    // descriptor sets
    uint32_t descriptor_set_count = 0;
    shader_module.EnumerateDescriptorSets(&descriptor_set_count, nullptr);
    Vector<SpvReflectDescriptorSet*> descriptor_sets(descriptor_set_count);
    if (descriptor_set_count > 0) {
        shader_module.EnumerateDescriptorSets(&descriptor_set_count, descriptor_sets.data());
    }

    for (SpvReflectDescriptorSet* set : descriptor_sets) {
        DescriptorSet descriptor_set = {};
        descriptor_set.set = set->set;

        for (uint32_t i = 0; i < set->binding_count; i++) {
            const SpvReflectDescriptorBinding* reflected = set->bindings[i];

            DescriptorBinding binding = {};

            binding.layout.binding = reflected->binding;
            binding.layout.descriptorType =
                static_cast<VkDescriptorType>(reflected->descriptor_type);

            uint32_t descriptor_count = 1;
            for (uint32_t d = 0; d < reflected->array.dims_count; d++)
                descriptor_count *= reflected->array.dims[d];

            binding.layout.descriptorCount = descriptor_count;
            binding.layout.stageFlags = stage;
            binding.layout.pImmutableSamplers = nullptr;

            binding.name = reflected->name ? reflected->name : "";
            binding.size = reflected->block.size;

            descriptor_set.bindings.push_back(binding);
        }

        shader_reflection.descriptor_sets.push_back(descriptor_set);
    }

    // push constants
    uint32_t push_constant_count = 0;
    shader_module.EnumeratePushConstantBlocks(&push_constant_count, nullptr);
    Vector<SpvReflectBlockVariable*> push_constants(push_constant_count);
    if (push_constant_count > 0) {
        shader_module.EnumeratePushConstantBlocks(&push_constant_count, push_constants.data());
    }
    for (SpvReflectBlockVariable* pc : push_constants) {
        PushConstantRange range = {};

        range.vk_range.stageFlags = stage;
        range.vk_range.offset = pc->offset;
        range.vk_range.size = pc->size;

        range.name = pc->name ? pc->name : "";
        shader_reflection.push_constants.push_back(range);
    }

    return shader_reflection;
}

void ShaderProgram::_build_vertex_info(Vector<SpvReflectInterfaceVariable*>& variables) {
    Vector<VkVertexInputAttributeDescription> attributes;

    uint32_t offset = 0;

    std::sort(variables.begin(), variables.end(),
              [](auto* a, auto* b) { return a->location < b->location; });

    for (SpvReflectInterfaceVariable* variable : variables) {
        if (variable->built_in != -1) continue;

        VkVertexInputAttributeDescription attr = {};
        attr.location = variable->location;
        attr.binding = 0;
        attr.format = (VkFormat)variable->format;
        attr.offset = offset;

        offset += vk_format_size(attr.format);
        attributes.push_back(attr);
    }

    VkVertexInputBindingDescription binding = {};
    binding.binding = 0;
    binding.stride = offset;
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    vk_vertex_info.bindings.clear();
    vk_vertex_info.bindings.push_back(binding);
    vk_vertex_info.attributes.clear();
    vk_vertex_info.attributes = attributes;

    VkPipelineVertexInputStateCreateInfo input_state_create_info = {};
    input_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    input_state_create_info.pNext = nullptr;
    input_state_create_info.flags = 0;
    input_state_create_info.vertexBindingDescriptionCount = vk_vertex_info.bindings.size();
    input_state_create_info.pVertexBindingDescriptions = vk_vertex_info.bindings.data();
    input_state_create_info.vertexAttributeDescriptionCount = vk_vertex_info.attributes.size();
    input_state_create_info.pVertexAttributeDescriptions = vk_vertex_info.attributes.data();

    vk_vertex_info.vk_state = input_state_create_info;
}

void ShaderProgram::_build_pipeline_layout(VkDevice vk_device) {
    HashMap<uint32_t, Vector<VkDescriptorSetLayoutBinding>> set_bindings;
    Vector<VkPushConstantRange> push_constant_ranges;

    uint32_t max_set = 0;

    for (const Ref<ShaderModuleAsset>& shader : shaders) {
        const ShaderReflection& reflection = shader->reflection;

        // descriptor sets
        for (const DescriptorSet& set : reflection.descriptor_sets) {
            max_set = std::max(max_set, set.set);

            auto& bindings = set_bindings[set.set];

            for (const DescriptorBinding& binding : set.bindings) {
                bool found = false;

                for (auto& existing : bindings) {
                    if (existing.binding == binding.layout.binding) {
                        existing.stageFlags |= binding.layout.stageFlags;

                        found = true;
                        ERR_FAIL_COND(existing.descriptorType != binding.layout.descriptorType);
                        ERR_FAIL_COND(existing.descriptorCount != binding.layout.descriptorCount);

                        break;
                    }
                }

                if (!found) {
                    bindings.push_back(binding.layout);
                }
            }
        }

        // push constants
        for (const PushConstantRange& range : reflection.push_constants) {
            push_constant_ranges.push_back(range.vk_range);
        }
    }

    for (auto& [set_index, bindings] : set_bindings) {
        VkDescriptorSetLayoutCreateInfo layout_info = {};
        layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layout_info.bindingCount = static_cast<uint32_t>(bindings.size());
        layout_info.pBindings = bindings.data();

        VkResult result = vkCreateDescriptorSetLayout(vk_device, &layout_info, nullptr,
                                                      &descriptor_set_layouts[set_index]);

        ERR_FAIL_COND(result != VK_SUCCESS);
    }

    VkPipelineLayoutCreateInfo pipeline_create_info = {};
    pipeline_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_create_info.pNext = nullptr;
    pipeline_create_info.flags = 0;
    pipeline_create_info.setLayoutCount = descriptor_set_layouts.size();
    pipeline_create_info.pSetLayouts = descriptor_set_layouts.data();
    pipeline_create_info.pushConstantRangeCount = push_constant_ranges.size();
    pipeline_create_info.pPushConstantRanges = push_constant_ranges.data();

    VkResult result =
        vkCreatePipelineLayout(vk_device, &pipeline_create_info, nullptr, &vk_pipeline_layout);
    ERR_FAIL_COND(result != VK_SUCCESS);
}

void ShaderProgram::_build_shader_module(VkDevice vk_device) {
    VkShaderModule shader_module = VK_NULL_HANDLE;

    for (const Ref<ShaderModuleAsset>& shader : shaders) {
        ShaderReflection reflection = shader->reflection;

        VkShaderModuleCreateInfo shader_module_create_info = {};
        shader_module_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        shader_module_create_info.pNext = nullptr;
        shader_module_create_info.flags = 0;
        shader_module_create_info.codeSize = reflection.source_size;
        shader_module_create_info.pCode = reflection.source;

        VkResult result =
            vkCreateShaderModule(vk_device, &shader_module_create_info, nullptr, &shader_module);
        ERR_FAIL_COND(result != VK_SUCCESS);
        vk_shader_modules.push_back(shader_module);

        VkPipelineShaderStageCreateInfo shader_stage_create_info = {};
        shader_stage_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shader_stage_create_info.pNext = nullptr;
        shader_stage_create_info.flags = 0;
        shader_stage_create_info.stage = (VkShaderStageFlagBits)reflection.stage;
        shader_stage_create_info.module = shader_module;
        shader_stage_create_info.pName = reflection.entry_point.c_str();

        vk_pipeline_stages.push_back(shader_stage_create_info);
    }
}
