#include "driver_device.hh"

#include "core/error/error_macros.hh"

Error RenderingDeviceDriver::initialize(uint32_t p_device_index, uint32_t p_frame_count) {
    context_device = context_driver->device_get(p_device_index);
    physical_device = context_driver->physical_device_get(p_device_index);

    vkGetPhysicalDeviceProperties(physical_device, &physical_device_properties);

    frame_count = p_frame_count;
    uint32_t queue_family_count = context_driver->queue_family_get_count(p_device_index);
    queue_family_properties.resize(queue_family_count);
    for (uint32_t i = 0; i < queue_family_count; i++) {
        queue_family_properties[i] = context_driver->queue_family_get(p_device_index, i);
    }

    Error err = _initialize_device_extensions();
    ERR_FAIL_COND_V(err != OK, err);

    err = _check_device_features();
    ERR_FAIL_COND_V(err != OK, err);

    err = _check_device_capabilities();
    ERR_FAIL_COND_V(err != OK, err);

    Vector<VkDeviceQueueCreateInfo> queue_create_info;
    err = _add_queue_create_info(queue_create_info);
    ERR_FAIL_COND_V(err != OK, err);

    err = _initialize_device(queue_create_info);
    ERR_FAIL_COND_V(err != OK, err);

    // device function pointers
    volkLoadDevice(vk_device);

    err = _initialize_allocator();
    ERR_FAIL_COND_V(err != OK, err);

    return OK;
}

void RenderingDeviceDriver::_register_requested_device_extension(const String &p_extension_name,
                                                                 bool p_required) {
    requested_device_extensions[p_extension_name] = p_required;
}

Error RenderingDeviceDriver::_initialize_device_extensions() {
    requested_device_extensions.clear();

    _register_requested_device_extension(VK_KHR_SWAPCHAIN_EXTENSION_NAME, true);
    _register_requested_device_extension(VK_KHR_MULTIVIEW_EXTENSION_NAME, false);
    _register_requested_device_extension(VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME, false);
    _register_requested_device_extension(VK_EXT_FRAGMENT_DENSITY_MAP_EXTENSION_NAME, false);
    _register_requested_device_extension(VK_EXT_FRAGMENT_DENSITY_MAP_2_EXTENSION_NAME, false);
    _register_requested_device_extension(VK_QCOM_FRAGMENT_DENSITY_MAP_OFFSET_EXTENSION_NAME, false);
    _register_requested_device_extension(VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME, false);
    _register_requested_device_extension(VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME, false);
    _register_requested_device_extension(VK_KHR_STORAGE_BUFFER_STORAGE_CLASS_EXTENSION_NAME, false);
    _register_requested_device_extension(VK_KHR_16BIT_STORAGE_EXTENSION_NAME, false);
    _register_requested_device_extension(VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME, false);
    _register_requested_device_extension(VK_KHR_MAINTENANCE_2_EXTENSION_NAME, false);
    _register_requested_device_extension(VK_EXT_PIPELINE_CREATION_CACHE_CONTROL_EXTENSION_NAME,
                                         false);
    _register_requested_device_extension(VK_EXT_SUBGROUP_SIZE_CONTROL_EXTENSION_NAME, false);
    _register_requested_device_extension(VK_EXT_ASTC_DECODE_MODE_EXTENSION_NAME, false);
    _register_requested_device_extension(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME, false);
    _register_requested_device_extension(VK_KHR_VULKAN_MEMORY_MODEL_EXTENSION_NAME, false);
    _register_requested_device_extension(VK_EXT_TEXTURE_COMPRESSION_ASTC_HDR_EXTENSION_NAME, false);
    _register_requested_device_extension(VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME, false);

    uint32_t device_extension_count = 0;
    VkResult err = vkEnumerateDeviceExtensionProperties(physical_device, nullptr,
                                                        &device_extension_count, nullptr);
    ERR_FAIL_COND_V(err != VK_SUCCESS, ERR_CANT_CREATE);
    ERR_FAIL_COND_V_MSG(device_extension_count == 0, ERR_CANT_CREATE,
                        "vkEnumerateDeviceExtensionProperties failed to find "
                        "any extensions\n\nDo you have a compatible Vulkan "
                        "installable client driver (ICD) installed?");

    Vector<VkExtensionProperties> device_extensions;
    device_extensions.resize(device_extension_count);
    err = vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &device_extension_count,
                                               device_extensions.data());
    ERR_FAIL_COND_V(err != VK_SUCCESS, ERR_CANT_CREATE);

    // Enable all extensions that are supported and requested.
    for (uint32_t i = 0; i < device_extension_count; i++) {
        String extension_name(device_extensions[i].extensionName);
        if (requested_device_extensions.find(extension_name) != requested_device_extensions.end()) {
            enabled_device_extension_names.insert(extension_name);
        }
    }

    for (const auto &[extension, required] : requested_device_extensions) {
        if (enabled_device_extension_names.find(extension) ==
            enabled_device_extension_names.end()) {
            if (required) {
                ERR_FAIL_V_MSG(ERR_CANT_CREATE, String("Required extension ") +
                                                    String::utf8(requested_extension.key) +
                                                    String(" not found."));
            }
        }
    }

    return OK;
}

Error RenderingDeviceDriver::_check_device_features() {
    vkGetPhysicalDeviceFeatures(physical_device, &physical_device_features);

    // check for required features
    if (!physical_device_features.imageCubeArray || !physical_device_features.independentBlend) {
        return ERR_CANT_CREATE;
    }

#define VK_DEVICEFEATURE_ENABLE_IF(x)                             \
    if (physical_device_features.x) {                             \
        requested_device_features.x = physical_device_features.x; \
    } else                                                        \
        ((void)0)

    requested_device_features = {};
    VK_DEVICEFEATURE_ENABLE_IF(fullDrawIndexUint32);
    VK_DEVICEFEATURE_ENABLE_IF(imageCubeArray);
    VK_DEVICEFEATURE_ENABLE_IF(independentBlend);
    VK_DEVICEFEATURE_ENABLE_IF(geometryShader);
    VK_DEVICEFEATURE_ENABLE_IF(tessellationShader);
    VK_DEVICEFEATURE_ENABLE_IF(sampleRateShading);
    VK_DEVICEFEATURE_ENABLE_IF(dualSrcBlend);
    VK_DEVICEFEATURE_ENABLE_IF(logicOp);
    VK_DEVICEFEATURE_ENABLE_IF(multiDrawIndirect);
    VK_DEVICEFEATURE_ENABLE_IF(drawIndirectFirstInstance);
    VK_DEVICEFEATURE_ENABLE_IF(depthClamp);
    VK_DEVICEFEATURE_ENABLE_IF(depthBiasClamp);
    VK_DEVICEFEATURE_ENABLE_IF(fillModeNonSolid);
    VK_DEVICEFEATURE_ENABLE_IF(depthBounds);
    VK_DEVICEFEATURE_ENABLE_IF(wideLines);
    VK_DEVICEFEATURE_ENABLE_IF(largePoints);
    VK_DEVICEFEATURE_ENABLE_IF(alphaToOne);
    VK_DEVICEFEATURE_ENABLE_IF(multiViewport);
    VK_DEVICEFEATURE_ENABLE_IF(samplerAnisotropy);
    VK_DEVICEFEATURE_ENABLE_IF(textureCompressionETC2);
    VK_DEVICEFEATURE_ENABLE_IF(textureCompressionASTC_LDR);
    VK_DEVICEFEATURE_ENABLE_IF(textureCompressionBC);
    VK_DEVICEFEATURE_ENABLE_IF(vertexPipelineStoresAndAtomics);
    VK_DEVICEFEATURE_ENABLE_IF(fragmentStoresAndAtomics);
    VK_DEVICEFEATURE_ENABLE_IF(shaderTessellationAndGeometryPointSize);
    VK_DEVICEFEATURE_ENABLE_IF(shaderImageGatherExtended);
    VK_DEVICEFEATURE_ENABLE_IF(shaderStorageImageExtendedFormats);
    VK_DEVICEFEATURE_ENABLE_IF(shaderStorageImageReadWithoutFormat);
    VK_DEVICEFEATURE_ENABLE_IF(shaderStorageImageWriteWithoutFormat);
    VK_DEVICEFEATURE_ENABLE_IF(shaderUniformBufferArrayDynamicIndexing);
    VK_DEVICEFEATURE_ENABLE_IF(shaderSampledImageArrayDynamicIndexing);
    VK_DEVICEFEATURE_ENABLE_IF(shaderStorageBufferArrayDynamicIndexing);
    VK_DEVICEFEATURE_ENABLE_IF(shaderStorageImageArrayDynamicIndexing);
    VK_DEVICEFEATURE_ENABLE_IF(shaderClipDistance);
    VK_DEVICEFEATURE_ENABLE_IF(shaderCullDistance);
    VK_DEVICEFEATURE_ENABLE_IF(shaderFloat64);
    VK_DEVICEFEATURE_ENABLE_IF(shaderInt64);
    VK_DEVICEFEATURE_ENABLE_IF(shaderInt16);
    VK_DEVICEFEATURE_ENABLE_IF(shaderResourceMinLod);
    VK_DEVICEFEATURE_ENABLE_IF(variableMultisampleRate);

    return OK;
}

Error RenderingDeviceDriver::_check_device_capabilities() {
#define HAS_EXTENSION(name) \
    (enabled_device_extension_names.find(name) != enabled_device_extension_names.end())

    if (HAS_EXTENSION(VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME)) {
        framebuffer_depth_resolve = true;
    }

    void *next_features = nullptr;
    VkPhysicalDeviceVulkan12Features device_features_vk_1_2 = {};
    VkPhysicalDeviceShaderFloat16Int8FeaturesKHR shader_features = {};
    VkPhysicalDeviceBufferDeviceAddressFeaturesKHR buffer_device_address_features = {};
    VkPhysicalDeviceVulkanMemoryModelFeaturesKHR vulkan_memory_model_features = {};
    VkPhysicalDeviceFragmentShadingRateFeaturesKHR fsr_features = {};
    VkPhysicalDeviceFragmentDensityMapFeaturesEXT fdm_features = {};
    VkPhysicalDeviceFragmentDensityMapOffsetFeaturesQCOM fdmo_features_qcom = {};
    VkPhysicalDevice16BitStorageFeaturesKHR storage_feature = {};
    VkPhysicalDeviceMultiviewFeatures multiview_features = {};
    VkPhysicalDevicePipelineCreationCacheControlFeatures pipeline_cache_control_features = {};
    VkPhysicalDeviceVulkanMemoryModelFeatures memory_model_features = {};

    const bool use_1_2_features = physical_device_properties.apiVersion >= VK_API_VERSION_1_2;
    if (use_1_2_features) {
        device_features_vk_1_2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
        device_features_vk_1_2.pNext = next_features;
        next_features = &device_features_vk_1_2;
    } else {
        if (HAS_EXTENSION(VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME)) {
            shader_features.sType =
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES_KHR;
            shader_features.pNext = next_features;
            next_features = &shader_features;
        }
        if (HAS_EXTENSION(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME)) {
            buffer_device_address_features.sType =
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_KHR;
            buffer_device_address_features.pNext = next_features;
            next_features = &buffer_device_address_features;
        }
        if (HAS_EXTENSION(VK_KHR_VULKAN_MEMORY_MODEL_EXTENSION_NAME)) {
            vulkan_memory_model_features.sType =
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_MEMORY_MODEL_FEATURES_KHR;
            vulkan_memory_model_features.pNext = next_features;
            next_features = &vulkan_memory_model_features;
        }
    }

    if (HAS_EXTENSION(VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME)) {
        fsr_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_FEATURES_KHR;
        fsr_features.pNext = next_features;
        next_features = &fsr_features;
    }

    if (HAS_EXTENSION(VK_EXT_FRAGMENT_DENSITY_MAP_EXTENSION_NAME)) {
        fdm_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_FEATURES_EXT;
        fdm_features.pNext = next_features;
        next_features = &fdm_features;
    }

    if (HAS_EXTENSION(VK_QCOM_FRAGMENT_DENSITY_MAP_OFFSET_EXTENSION_NAME)) {
        fdmo_features_qcom.sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_OFFSET_FEATURES_QCOM;
        fdmo_features_qcom.pNext = next_features;
        next_features = &fdmo_features_qcom;
    }

    if (HAS_EXTENSION(VK_KHR_16BIT_STORAGE_EXTENSION_NAME)) {
        storage_feature.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES_KHR;
        storage_feature.pNext = next_features;
        next_features = &storage_feature;
    }

    if (HAS_EXTENSION(VK_KHR_MULTIVIEW_EXTENSION_NAME)) {
        multiview_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES;
        multiview_features.pNext = next_features;
        next_features = &multiview_features;
    }

    if (HAS_EXTENSION(VK_EXT_PIPELINE_CREATION_CACHE_CONTROL_EXTENSION_NAME)) {
        pipeline_cache_control_features.sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_CREATION_CACHE_CONTROL_FEATURES;
        pipeline_cache_control_features.pNext = next_features;
        next_features = &pipeline_cache_control_features;
    }

    if (HAS_EXTENSION(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME)) {
        memory_model_features.sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_MEMORY_MODEL_FEATURES;
        memory_model_features.pNext = next_features;
        next_features = &memory_model_features;
    }

    VkPhysicalDeviceFeatures2 device_features_2 = {};
    device_features_2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    device_features_2.pNext = next_features;

    if (use_1_2_features) {
        if (HAS_EXTENSION(VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME)) {
            shader_capabilities.shader_float16_is_supported = device_features_vk_1_2.shaderFloat16;
            shader_capabilities.shader_int8_is_supported = device_features_vk_1_2.shaderInt8;
        }
        if (HAS_EXTENSION(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME)) {
            buffer_device_address_support = device_features_vk_1_2.bufferDeviceAddress;
        }
        if (HAS_EXTENSION(VK_KHR_VULKAN_MEMORY_MODEL_EXTENSION_NAME)) {
            vulkan_memory_model_support = device_features_vk_1_2.vulkanMemoryModel;
            vulkan_memory_model_device_scope_support =
                device_features_vk_1_2.vulkanMemoryModelDeviceScope;
        }
    } else {
        if (HAS_EXTENSION(VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME)) {
            shader_capabilities.shader_float16_is_supported = shader_features.shaderFloat16;
            shader_capabilities.shader_int8_is_supported = shader_features.shaderInt8;
        }
        if (HAS_EXTENSION(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME)) {
            buffer_device_address_support = buffer_device_address_features.bufferDeviceAddress;
        }
        if (HAS_EXTENSION(VK_KHR_VULKAN_MEMORY_MODEL_EXTENSION_NAME)) {
            vulkan_memory_model_support = vulkan_memory_model_features.vulkanMemoryModel;
            vulkan_memory_model_device_scope_support =
                vulkan_memory_model_features.vulkanMemoryModelDeviceScope;
        }
    }

    if (HAS_EXTENSION(VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME)) {
        fsr_capabilities.pipeline_supported = fsr_features.pipelineFragmentShadingRate;
        fsr_capabilities.primitive_supported = fsr_features.primitiveFragmentShadingRate;
        fsr_capabilities.attachment_supported = fsr_features.attachmentFragmentShadingRate;
    }

    if (HAS_EXTENSION(VK_EXT_FRAGMENT_DENSITY_MAP_EXTENSION_NAME)) {
        fdm_capabilities.attachment_supported = fdm_features.fragmentDensityMap;
        fdm_capabilities.dynamic_attachment_supported = fdm_features.fragmentDensityMapDynamic;
        fdm_capabilities.non_subsampled_images_supported =
            fdm_features.fragmentDensityMapNonSubsampledImages;
    }

    if (HAS_EXTENSION(VK_QCOM_FRAGMENT_DENSITY_MAP_OFFSET_EXTENSION_NAME)) {
        fdm_capabilities.offset_supported = fdmo_features_qcom.fragmentDensityMapOffset;
    }

    if (HAS_EXTENSION(VK_KHR_MULTIVIEW_EXTENSION_NAME)) {
        multiview_capabilities.is_supported = multiview_features.multiview;
        multiview_capabilities.geometry_shader_is_supported =
            multiview_features.multiviewGeometryShader;
        multiview_capabilities.tessellation_shader_is_supported =
            multiview_features.multiviewTessellationShader;
    }

    if (HAS_EXTENSION(VK_KHR_16BIT_STORAGE_EXTENSION_NAME)) {
        storage_buffer_capabilities.storage_buffer_16_bit_access_is_supported =
            storage_feature.storageBuffer16BitAccess;
        storage_buffer_capabilities.uniform_and_storage_buffer_16_bit_access_is_supported =
            storage_feature.uniformAndStorageBuffer16BitAccess;
        storage_buffer_capabilities.storage_push_constant_16_is_supported =
            storage_feature.storagePushConstant16;
        storage_buffer_capabilities.storage_input_output_16 = storage_feature.storageInputOutput16;
    }

    if (HAS_EXTENSION(VK_EXT_PIPELINE_CREATION_CACHE_CONTROL_EXTENSION_NAME)) {
        pipeline_cache_control_support =
            pipeline_cache_control_features.pipelineCreationCacheControl;
    }

    if (HAS_EXTENSION(VK_EXT_DEVICE_FAULT_EXTENSION_NAME)) {
        device_fault_support = true;
    }

    void *next_properties = nullptr;
    VkPhysicalDeviceFragmentShadingRatePropertiesKHR fsr_properties = {};
    VkPhysicalDeviceFragmentDensityMapPropertiesEXT fdm_properties = {};
    VkPhysicalDeviceFragmentDensityMapOffsetPropertiesQCOM fdmo_properties = {};
    VkPhysicalDeviceMultiviewProperties multiview_properties = {};
    VkPhysicalDeviceSubgroupProperties subgroup_properties = {};
    VkPhysicalDeviceSubgroupSizeControlProperties subgroup_size_control_properties = {};
    VkPhysicalDeviceAccelerationStructurePropertiesKHR acceleration_structure_properties = {};
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR raytracing_properties = {};
    VkPhysicalDeviceProperties2 physical_device_properties_2 = {};

    const bool use_1_1_properties = physical_device_properties.apiVersion >= VK_API_VERSION_1_1;
    if (use_1_1_properties) {
        subgroup_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES;
        subgroup_properties.pNext = next_properties;
        next_properties = &subgroup_properties;

        subgroup_capabilities.size_control_is_supported =
            HAS_EXTENSION(VK_EXT_SUBGROUP_SIZE_CONTROL_EXTENSION_NAME);
        if (subgroup_capabilities.size_control_is_supported) {
            subgroup_size_control_properties.sType =
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_PROPERTIES;
            subgroup_size_control_properties.pNext = next_properties;
            next_properties = &subgroup_size_control_properties;
        }
    }

    if (multiview_capabilities.is_supported) {
        multiview_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_PROPERTIES;
        multiview_properties.pNext = next_properties;
        next_properties = &multiview_properties;
    }

    if (fsr_capabilities.attachment_supported) {
        fsr_properties.sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_PROPERTIES_KHR;
        fsr_properties.pNext = next_properties;
        next_properties = &fsr_properties;
    }

    if (fdm_capabilities.attachment_supported) {
        fdm_properties.sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_PROPERTIES_EXT;
        fdm_properties.pNext = next_properties;
        next_properties = &fdm_properties;
    }

    if (fdm_capabilities.offset_supported) {
        fdmo_properties.sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_OFFSET_PROPERTIES_QCOM;
        fdmo_properties.pNext = next_properties;
        next_properties = &fdmo_properties;
    }

    physical_device_properties_2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    physical_device_properties_2.pNext = next_properties;

    subgroup_capabilities.size = subgroup_properties.subgroupSize;
    subgroup_capabilities.min_size = subgroup_properties.subgroupSize;
    subgroup_capabilities.max_size = subgroup_properties.subgroupSize;
    subgroup_capabilities.supported_stages = subgroup_properties.supportedStages;
    subgroup_capabilities.supported_operations = subgroup_properties.supportedOperations;
    subgroup_capabilities.quad_operations_in_all_stages =
        subgroup_properties.quadOperationsInAllStages;

    if (subgroup_capabilities.size_control_is_supported &&
        (subgroup_size_control_properties.requiredSubgroupSizeStages &
         VK_SHADER_STAGE_COMPUTE_BIT)) {
        subgroup_capabilities.min_size = subgroup_size_control_properties.minSubgroupSize;
        subgroup_capabilities.max_size = subgroup_size_control_properties.maxSubgroupSize;
    }

    return OK;
}

Error RenderingDeviceDriver::_add_queue_create_info(
    Vector<VkDeviceQueueCreateInfo> &r_queue_create_info) {
    uint32_t queue_family_count = queue_family_properties.size();
    queue_families.resize(queue_family_count);

    VkQueueFlags queue_flags_mask =
        VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT;
    const uint32_t max_queue_count_per_family = 1;
    static const float queue_priorities[max_queue_count_per_family] = {};

    for (uint32_t i = 0; i < queue_family_count; i++) {
        if ((queue_family_properties[i].queueFlags & queue_flags_mask) == 0) {
            // We ignore creating queues in families that don't support any of
            // the operations we require.
            continue;
        }

        VkDeviceQueueCreateInfo create_info = {};
        create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        create_info.queueFamilyIndex = i;
        create_info.queueCount =
            std::min(queue_family_properties[i].queueCount, max_queue_count_per_family);
        create_info.pQueuePriorities = queue_priorities;
        r_queue_create_info.push_back(create_info);

        // Prepare the vectors where the queues will be filled out.
        queue_families[i].resize(create_info.queueCount);
    }

    return OK;
}

Error RenderingDeviceDriver::_initialize_device(
    const Vector<VkDeviceQueueCreateInfo> &p_queue_create_info) {
    Vector<const char *> enabled_extension_names;
    enabled_extension_names.reserve(enabled_device_extension_names.size());
    for (const String &extension_name : enabled_device_extension_names) {
        enabled_extension_names.push_back(extension_name.data());
    }

    void *create_info_next = nullptr;
    VkPhysicalDeviceShaderFloat16Int8FeaturesKHR shader_features = {};
    shader_features.pNext = create_info_next;
    shader_features.shaderFloat16 = shader_capabilities.shader_float16_is_supported;
    shader_features.shaderInt8 = shader_capabilities.shader_int8_is_supported;
    create_info_next = &shader_features;

    VkPhysicalDeviceBufferDeviceAddressFeaturesKHR buffer_device_address_features = {};
    if (buffer_device_address_support) {
        buffer_device_address_features.sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_KHR;
        buffer_device_address_features.pNext = create_info_next;
        buffer_device_address_features.bufferDeviceAddress = buffer_device_address_support;
        create_info_next = &buffer_device_address_features;
    }

    VkPhysicalDeviceVulkanMemoryModelFeaturesKHR vulkan_memory_model_features = {};
    if (vulkan_memory_model_support && vulkan_memory_model_device_scope_support) {
        vulkan_memory_model_features.sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_MEMORY_MODEL_FEATURES_KHR;
        vulkan_memory_model_features.pNext = create_info_next;
        vulkan_memory_model_features.vulkanMemoryModel = vulkan_memory_model_support;
        vulkan_memory_model_features.vulkanMemoryModelDeviceScope =
            vulkan_memory_model_device_scope_support;
        create_info_next = &vulkan_memory_model_features;
    }

    VkPhysicalDeviceFragmentShadingRateFeaturesKHR fsr_features = {};
    if (fsr_capabilities.pipeline_supported || fsr_capabilities.primitive_supported ||
        fsr_capabilities.attachment_supported) {
        fsr_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_FEATURES_KHR;
        fsr_features.pNext = create_info_next;
        fsr_features.pipelineFragmentShadingRate = fsr_capabilities.pipeline_supported;
        fsr_features.primitiveFragmentShadingRate = fsr_capabilities.primitive_supported;
        fsr_features.attachmentFragmentShadingRate = fsr_capabilities.attachment_supported;
        create_info_next = &fsr_features;
    }

    VkPhysicalDeviceFragmentDensityMapFeaturesEXT fdm_features = {};
    if (fdm_capabilities.attachment_supported || fdm_capabilities.dynamic_attachment_supported ||
        fdm_capabilities.non_subsampled_images_supported) {
        fdm_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_FEATURES_EXT;
        fdm_features.pNext = create_info_next;
        fdm_features.fragmentDensityMap = fdm_capabilities.attachment_supported;
        fdm_features.fragmentDensityMapDynamic = fdm_capabilities.dynamic_attachment_supported;
        fdm_features.fragmentDensityMapNonSubsampledImages =
            fdm_capabilities.non_subsampled_images_supported;
        create_info_next = &fdm_features;
    }

    VkPhysicalDeviceFragmentDensityMapOffsetFeaturesQCOM fdm_offset_features = {};
    if (fdm_capabilities.offset_supported) {
        fdm_offset_features.sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_OFFSET_FEATURES_QCOM;
        fdm_offset_features.pNext = create_info_next;
        fdm_offset_features.fragmentDensityMapOffset = VK_TRUE;
        create_info_next = &fdm_offset_features;
    }

    VkPhysicalDevicePipelineCreationCacheControlFeatures pipeline_cache_control_features = {};
    if (pipeline_cache_control_support) {
        pipeline_cache_control_features.sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_CREATION_CACHE_CONTROL_FEATURES;
        pipeline_cache_control_features.pNext = create_info_next;
        pipeline_cache_control_features.pipelineCreationCacheControl =
            pipeline_cache_control_support;
        create_info_next = &pipeline_cache_control_features;
    }

    VkPhysicalDeviceFaultFeaturesEXT device_fault_features = {};
    if (device_fault_support) {
        device_fault_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FAULT_FEATURES_EXT;
        device_fault_features.pNext = create_info_next;
        create_info_next = &device_fault_features;
    }

    VkPhysicalDeviceVulkan11Features vulkan_1_1_features = {};
    VkPhysicalDevice16BitStorageFeaturesKHR storage_features = {};
    VkPhysicalDeviceMultiviewFeatures multiview_features = {};
    const bool enable_1_2_features = physical_device_properties.apiVersion >= VK_API_VERSION_1_2;
    if (enable_1_2_features) {
        vulkan_1_1_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
        vulkan_1_1_features.pNext = create_info_next;
        vulkan_1_1_features.storageBuffer16BitAccess =
            storage_buffer_capabilities.storage_buffer_16_bit_access_is_supported;
        vulkan_1_1_features.uniformAndStorageBuffer16BitAccess =
            storage_buffer_capabilities.uniform_and_storage_buffer_16_bit_access_is_supported;
        vulkan_1_1_features.storagePushConstant16 =
            storage_buffer_capabilities.storage_push_constant_16_is_supported;
        vulkan_1_1_features.storageInputOutput16 =
            storage_buffer_capabilities.storage_input_output_16;
        vulkan_1_1_features.multiview = multiview_capabilities.is_supported;
        vulkan_1_1_features.multiviewGeometryShader =
            multiview_capabilities.geometry_shader_is_supported;
        vulkan_1_1_features.multiviewTessellationShader =
            multiview_capabilities.tessellation_shader_is_supported;
        vulkan_1_1_features.variablePointersStorageBuffer = 0;
        vulkan_1_1_features.variablePointers = 0;
        vulkan_1_1_features.protectedMemory = 0;
        vulkan_1_1_features.samplerYcbcrConversion = 0;
        vulkan_1_1_features.shaderDrawParameters = 0;
        create_info_next = &vulkan_1_1_features;
    } else {
        storage_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES_KHR;
        storage_features.pNext = create_info_next;
        storage_features.storageBuffer16BitAccess =
            storage_buffer_capabilities.storage_buffer_16_bit_access_is_supported;
        storage_features.uniformAndStorageBuffer16BitAccess =
            storage_buffer_capabilities.uniform_and_storage_buffer_16_bit_access_is_supported;
        storage_features.storagePushConstant16 =
            storage_buffer_capabilities.storage_push_constant_16_is_supported;
        storage_features.storageInputOutput16 = storage_buffer_capabilities.storage_input_output_16;
        create_info_next = &storage_features;

        const bool enable_1_1_features =
            physical_device_properties.apiVersion >= VK_API_VERSION_1_1;
        if (enable_1_1_features) {
            multiview_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES;
            multiview_features.pNext = create_info_next;
            multiview_features.multiview = multiview_capabilities.is_supported;
            multiview_features.multiviewGeometryShader =
                multiview_capabilities.geometry_shader_is_supported;
            multiview_features.multiviewTessellationShader =
                multiview_capabilities.tessellation_shader_is_supported;
            create_info_next = &multiview_features;
        }
    };

    VkDeviceCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.pNext = create_info_next;
    create_info.queueCreateInfoCount = p_queue_create_info.size();
    create_info.pQueueCreateInfos = p_queue_create_info.data();
    create_info.enabledExtensionCount = enabled_extension_names.size();
    create_info.ppEnabledExtensionNames = enabled_extension_names.data();
    create_info.pEnabledFeatures = &requested_device_features;

    VkResult err = vkCreateDevice(physical_device, &create_info, nullptr, &vk_device);
    ERR_FAIL_COND_V(err != VK_SUCCESS, ERR_CANT_CREATE);

    for (uint32_t i = 0; i < queue_families.size(); i++) {
        for (uint32_t j = 0; j < queue_families[i].size(); j++) {
            vkGetDeviceQueue(vk_device, i, j, &queue_families[i][j].queue);
        }
    }

    return OK;
}

Error RenderingDeviceDriver::_initialize_allocator() {
    VmaAllocatorCreateInfo allocator_info = {};
    allocator_info.physicalDevice = physical_device;
    allocator_info.device = vk_device;
    allocator_info.instance = context_driver->instance_get();
    const bool use_1_3_features = physical_device_properties.apiVersion >= VK_API_VERSION_1_3;
    if (use_1_3_features) {
        allocator_info.flags |= VMA_ALLOCATOR_CREATE_KHR_MAINTENANCE5_BIT;
    }
    if (buffer_device_address_support) {
        allocator_info.flags |= VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    }

    VmaVulkanFunctions vulkan_functions = {};
    vulkan_functions.vkGetPhysicalDeviceProperties = vkGetPhysicalDeviceProperties;
    vulkan_functions.vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties;
    vulkan_functions.vkAllocateMemory = vkAllocateMemory;
    vulkan_functions.vkFreeMemory = vkFreeMemory;
    vulkan_functions.vkMapMemory = vkMapMemory;
    vulkan_functions.vkUnmapMemory = vkUnmapMemory;
    vulkan_functions.vkFlushMappedMemoryRanges = vkFlushMappedMemoryRanges;
    vulkan_functions.vkInvalidateMappedMemoryRanges = vkInvalidateMappedMemoryRanges;
    vulkan_functions.vkBindBufferMemory = vkBindBufferMemory;
    vulkan_functions.vkBindImageMemory = vkBindImageMemory;
    vulkan_functions.vkGetBufferMemoryRequirements = vkGetBufferMemoryRequirements;
    vulkan_functions.vkGetImageMemoryRequirements = vkGetImageMemoryRequirements;
    vulkan_functions.vkCreateBuffer = vkCreateBuffer;
    vulkan_functions.vkDestroyBuffer = vkDestroyBuffer;
    vulkan_functions.vkCreateImage = vkCreateImage;
    vulkan_functions.vkDestroyImage = vkDestroyImage;
    vulkan_functions.vkCmdCopyBuffer = vkCmdCopyBuffer;

    allocator_info.pVulkanFunctions = &vulkan_functions;

    VkResult err = vmaCreateAllocator(&allocator_info, &allocator);
    ERR_FAIL_COND_V_MSG(err != VK_SUCCESS, ERR_CANT_CREATE, "vmaCreateAllocator failed");

    return OK;
}

/* Buffer */
Ref<Buffer> RenderingDeviceDriver::buffer_create(uint64_t p_size, VkBufferUsageFlags p_usage,
                                                 VmaMemoryUsage p_vma_usage) {
    Ref<Buffer> buffer = new Buffer();
    buffer->size = p_size;

    VkBufferCreateInfo buffer_info = {};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = p_size;
    buffer_info.usage = p_usage;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo alloc_info = {};
    alloc_info.usage = p_vma_usage;

    VkResult result = vmaCreateBuffer(allocator, &buffer_info, &alloc_info, &buffer->vk_buffer,
                                      &buffer->allocation.handle, nullptr);
    ERR_FAIL_COND_V_MSG(result != VK_SUCCESS, nullptr, "Failed to create buffer");

    return buffer;
}

void RenderingDeviceDriver::buffer_free(Ref<Buffer> p_buffer, VkFormat p_format) {
    vmaDestroyBuffer(allocator, p_buffer->vk_buffer, p_buffer->allocation.handle);
}

uint64_t RenderingDeviceDriver::buffer_get_allocation_size(Ref<Buffer> p_buffer) { return 0; }

uint8_t *RenderingDeviceDriver::buffer_map(Ref<Buffer> p_buffer) { return nullptr; }

void RenderingDeviceDriver::buffer_unmap(Ref<Buffer> p_buffer) {}

RenderingDeviceDriver::RenderingDeviceDriver(RenderingDriverContext *p_context_driver)
    : context_driver(p_context_driver) {}

RenderingDeviceDriver::~RenderingDeviceDriver() {}
