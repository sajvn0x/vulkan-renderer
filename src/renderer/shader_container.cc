#include "shader_container.hh"

#include <spirv_reflect.h>

#include "core/error/error_macros.hh"
#include "core/fs/fs.hh"

const uint32_t MAX_SHADER_FILES_PER_PROGRAM = 5;

Error ShaderProgram::load_shader_program(const String& shader_name) {
    Vector<Path> shader_files = FileSystem::find_shader_files_by_name(shader_name);
    ERR_FAIL_COND_V(shader_files.size() > MAX_SHADER_FILES_PER_PROGRAM, FAILED);
    Error err = OK;

    for (Path& shader : shader_files) {
        err = _build_shader_module_asset(shader);
        ERR_FAIL_COND_V(err != OK, err);
    }

    if (pipeline_type == PIPELINE_TYPE_GRAPHICS) {
        err = _build_vertex_info();
        ERR_FAIL_COND_V(err != OK, err);
    }

    return err;
}

Error ShaderProgram::_build_shader_module_asset(const String& name) {
    Ref<ShaderModuleAsset> shader_module_asset = new ShaderModuleAsset();

    ShaderReflection* reflection = _shader_reflection(name);
    ERR_FAIL_COND_V(!reflection, FAILED);

    shader_module_asset->source_path = name;
    shader_module_asset->reflection = *reflection;

    shaders.push_back(shader_module_asset);

    return OK;
}

ShaderReflection* ShaderProgram::_shader_reflection(const String& name) {
    ShaderReflection shader_reflection;

    SpirvFileResult file_result = FileSystem::read_spirv_file(name);
    ERR_FAIL_COND_V(!file_result.success, nullptr);

    spv_reflect::ShaderModule shader_module =
        spv_reflect::ShaderModule(file_result.size, file_result.content.data());

    ERR_FAIL_COND_V(shader_module.GetResult() != SPV_REFLECT_RESULT_SUCCESS, nullptr);

    // entry point
    shader_reflection.entry_point = shader_module.GetEntryPointName();

    // shader stage
    ERR_FAIL_COND_V_MSG(
        (VkShaderStageFlags)shader_module.GetShaderStage() > VK_SHADER_STAGE_COMPUTE_BIT, nullptr,
        "For now, we only support primitive shaders, and compute shaders");
    shader_reflection.stage = shader_module.GetShaderStage();

    // input variables
    uint32_t input_variable_count = 0;
    shader_module.EnumerateInputVariables(&input_variable_count, nullptr);
    Vector<SpvReflectInterfaceVariable*> input_variables(input_variable_count);
    shader_module.EnumerateInputVariables(&input_variable_count, input_variables.data());
    for (SpvReflectInterfaceVariable* input_variable : input_variables) {
        printf("variable: %s\n", input_variable->name);
    }

    // descriptor sets
    uint32_t descriptor_set_count = 0;
    shader_module.EnumerateDescriptorSets(&descriptor_set_count, nullptr);
    Vector<SpvReflectDescriptorSet*> descriptor_sets(descriptor_set_count);
    shader_module.EnumerateDescriptorSets(&descriptor_set_count, descriptor_sets.data());
    for (SpvReflectDescriptorSet* descriptor_set : descriptor_sets) {
        printf("descriptor set: %d\n", descriptor_set->binding_count);
    }

    // descriptor bindings
    uint32_t descriptor_binding_count = 0;
    shader_module.EnumerateDescriptorBindings(&descriptor_binding_count, nullptr);
    Vector<SpvReflectDescriptorBinding*> reflect_descriptor_bindings(descriptor_binding_count);
    shader_module.EnumerateDescriptorBindings(&descriptor_binding_count,
                                              reflect_descriptor_bindings.data());
    for (SpvReflectDescriptorBinding* reflected : reflect_descriptor_bindings) {
		DescriptorBinding descriptor_binding = {};
		descriptor_binding.layout.binding = reflected->binding;
		descriptor_binding.layout.descriptorType = (VkDescriptorType)reflected->descriptor_type;
		descriptor_binding.layout.descriptorCount = 1;

		for (uint32_t i = 0; i < reflected->array.dims_count; ++i) {
			descriptor_binding.layout.descriptorCount *= reflected->array.dims[i];
		}
		descriptor_binding.layout.pImmutableSamplers = nullptr;

        printf("descriptor binding: %s\n", reflected->name);
    }

    // push constants
    uint32_t push_constant_count = 0;
    shader_module.EnumeratePushConstantBlocks(&push_constant_count, nullptr);
    Vector<SpvReflectBlockVariable*> push_constants(push_constant_count);
    shader_module.EnumeratePushConstantBlocks(&push_constant_count, push_constants.data());
    for (SpvReflectBlockVariable* push_constant : push_constants) {
        printf("push constant: %s\n", push_constant->name);
    }

    return std::move(&shader_reflection);
}

Error ShaderProgram::_build_vertex_info() {
    ERR_FAIL_COND_V_MSG(pipeline_type == PIPELINE_TYPE_COMPUTE, FAILED,
                        "Compute pipeline doesn't need vertex info");

    return OK;
}

/*
VkDevice vk_device = VK_NULL_HANDLE;
Error ShaderProgram::_build_pipeline_layout() {
        VkPipelineLayoutCreateInfo pipeline_layout_create_info = {};
        pipeline_layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipeline_layout_create_info.pNext = nullptr;
        pipeline_layout_create_info.flags = 0;
        pipeline_layout_create_info.setLayoutCount = ;
        pipeline_layout_create_info.pSetLayouts = ;
        pipeline_layout_create_info.pushConstantRangeCount = ;
        pipeline_layout_create_info.pPushConstantRanges = ;

        VkResult result = vkCreatePipelineLayout(vk_device, &pipeline_layout_create_info, nullptr,
&vk_pipeline_layout);

        return OK;
}
*/
