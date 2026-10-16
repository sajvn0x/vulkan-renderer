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

Error ShaderProgram::load_shader_program(const String &shader_name, VkDevice vk_device) {
    Vector<Path> shader_files = FileSystem::find_shader_files_by_name(shader_name);
    ERR_FAIL_COND_V(shader_files.size() > MAX_SHADER_FILES_PER_PROGRAM || shader_files.size() == 0,
                    FAILED);
    Error err = OK;

    // detect the pipeline type
    if (shader_files.size() == 1) {
        pipeline_type = PIPELINE_TYPE_COMPUTE;
    } else if (shader_files.size() >= 2) {
        pipeline_type = PIPELINE_TYPE_GRAPHICS;
    };

    for (Path &shader : shader_files) {
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

    err = _build_pipeline_layout(vk_device);
    ERR_FAIL_COND_V(err != OK, err);

    _build_shader_module(vk_device);

    return err;
}

Error ShaderProgram::_build_shader_module_asset(const String &name) {
    Ref<ShaderModuleAsset> shader_module_asset = new ShaderModuleAsset();

    SpirvFileResult file_result = FileSystem::read_spirv_file(name);
    ERR_FAIL_COND_V(!file_result.success, FAILED);

    shader_module_asset->source.resize(file_result.size);
    shader_module_asset->source = file_result.content;
    shader_module_asset->source_size = file_result.size;
    shader_module_asset->source_path = name;
    shader_module_asset->reflection = spv_reflect::ShaderModule(shader_module_asset->source_size,
                                                                shader_module_asset->source.data());

    ERR_FAIL_COND_V(shader_module_asset->reflection.GetResult() != SPV_REFLECT_RESULT_SUCCESS,
                    FAILED);

    spv_reflect::ShaderModule &shader_module = shader_module_asset->reflection;

    VkShaderStageFlags stage = (VkShaderStageFlagBits)shader_module.GetShaderStage();

    // input variables (vertex only)
    uint32_t input_variable_count = 0;
    shader_module.EnumerateInputVariables(&input_variable_count, nullptr);
    Vector<SpvReflectInterfaceVariable *> input_variables(input_variable_count);
    shader_module.EnumerateInputVariables(&input_variable_count, input_variables.data());

    switch (stage) {
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
    if (descriptor_set_count != 0) {
        Vector<SpvReflectDescriptorSet *> spv_descriptor_sets(descriptor_set_count);
        shader_module.EnumerateDescriptorSets(&descriptor_set_count, spv_descriptor_sets.data());

        for (SpvReflectDescriptorSet *spv_descriptor_set : spv_descriptor_sets) {
            DescriptorSet descriptor_set = {};
            shader_module_asset->descriptor_sets[spv_descriptor_set->set] = {};

            for (uint32_t i = 0; i < spv_descriptor_set->binding_count; ++i) {
                DescriptorBinding binding = {};

                binding.binding = spv_descriptor_set->bindings[i]->binding;
                binding.descriptor_count = spv_descriptor_set->bindings[i]->count;
                binding.descriptor_type =
                    (VkDescriptorType)spv_descriptor_set->bindings[i]->descriptor_type;
                binding.stage_flag = stage;

                shader_module_asset->descriptor_sets[spv_descriptor_set->set]
                    .bindings[spv_descriptor_set->bindings[i]->binding] = binding;
            }
        }
    }

    // push constants
    uint32_t push_constants_count = 0;
    shader_module.EnumeratePushConstantBlocks(&push_constants_count, nullptr);
    if (push_constants_count > 0) {
        Vector<SpvReflectBlockVariable *> push_constants(push_constants_count);
        shader_module.EnumeratePushConstantBlocks(&push_constants_count, push_constants.data());

        for (SpvReflectBlockVariable *pc : push_constants) {
            PushConstantRange range = {};

            range.vk_range.stageFlags = stage;
            range.vk_range.offset = pc->offset;
            range.vk_range.size = pc->size;

            range.name = pc->name ? pc->name : "";
            shader_module_asset->push_constants.push_back(range);
        }
    }

    shader_module_asset->stage = stage;
    shader_module_asset->entry_point = shader_module.GetEntryPointName();
    shaders.push_back(shader_module_asset);

    return OK;
}

void ShaderProgram::_build_vertex_info(Vector<SpvReflectInterfaceVariable *> &variables) {
    Vector<VkVertexInputAttributeDescription> attributes;

    uint32_t offset = 0;

    std::sort(variables.begin(), variables.end(),
              [](auto *a, auto *b) { return a->location < b->location; });

    for (SpvReflectInterfaceVariable *variable : variables) {
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

Error ShaderProgram::_build_pipeline_layout(VkDevice vk_device) {
    HashMap<uint32_t, HashMap<uint32_t, VkDescriptorSetLayoutBinding>> set_bindings;
    Vector<VkPushConstantRange> push_constant_ranges;

    for (const Ref<ShaderModuleAsset> &shader : shaders) {
        VkShaderStageFlags shader_stage = shader->stage;

        // descriptor sets
        for (const auto &[set_index, descriptor_set] : shader->descriptor_sets) {
            auto &bindings = set_bindings[set_index];

            for (const auto &[binding_index, descriptor_binding] : descriptor_set.bindings) {
                auto it = bindings.find(binding_index);
                if (it != bindings.end()) {
                    auto &existing = it->second;

                    if (existing.descriptorType != descriptor_binding.descriptor_type ||
                        existing.descriptorCount != descriptor_binding.descriptor_count) {
                        ERR_FAIL_V_MSG(FAILED,
                                       "Incompatible descriptors were used across shader program");
                    }

                    existing.stageFlags |= shader_stage;

                } else {
                    VkDescriptorSetLayoutBinding binding = {};
                    binding.binding = descriptor_binding.binding;
                    binding.descriptorType = descriptor_binding.descriptor_type;
                    binding.descriptorCount = descriptor_binding.descriptor_count;
                    binding.stageFlags = shader_stage;

                    bindings[binding_index] = binding;
                }
            }
        }
    }

    descriptor_set_layouts.resize(set_bindings.size());

    for (const auto &[set_index, bindings] : set_bindings) {
        Vector<VkDescriptorSetLayoutBinding> layout_bindings;

        for (const auto &[binding_index, binding] : bindings) {
            layout_bindings.push_back(binding);
        }

        VkDescriptorSetLayoutCreateInfo layout_info{};
        layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layout_info.bindingCount = static_cast<uint32_t>(layout_bindings.size());
        layout_info.pBindings = layout_bindings.data();

        VkDescriptorSetLayout layout = VK_NULL_HANDLE;

        VkResult result = vkCreateDescriptorSetLayout(vk_device, &layout_info, nullptr, &layout);
        ERR_FAIL_COND_V_MSG(result, FAILED, "Failed to create descriptor set layout");

        descriptor_set_layouts[set_index] = layout;
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
    ERR_FAIL_COND_V_MSG(result, FAILED, "Failed to create pipeline layout");

    return OK;
}

void ShaderProgram::_build_shader_module(VkDevice vk_device) {
    VkShaderModule shader_module = VK_NULL_HANDLE;

    for (const Ref<ShaderModuleAsset> &shader : shaders) {
        VkShaderModuleCreateInfo shader_module_create_info = {};
        shader_module_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        shader_module_create_info.pNext = nullptr;
        shader_module_create_info.flags = 0;
        shader_module_create_info.codeSize = shader->source_size;
        shader_module_create_info.pCode = shader->source.data();

        VkResult result =
            vkCreateShaderModule(vk_device, &shader_module_create_info, nullptr, &shader_module);
        ERR_FAIL_COND(result != VK_SUCCESS);
        vk_shader_modules.push_back(shader_module);

        VkPipelineShaderStageCreateInfo shader_stage_create_info = {};
        shader_stage_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shader_stage_create_info.pNext = nullptr;
        shader_stage_create_info.flags = 0;
        shader_stage_create_info.stage = (VkShaderStageFlagBits)shader->stage;
        shader_stage_create_info.module = shader_module;
        shader_stage_create_info.pName = shader->entry_point.c_str();

        vk_pipeline_stages.push_back(shader_stage_create_info);
    }
}
