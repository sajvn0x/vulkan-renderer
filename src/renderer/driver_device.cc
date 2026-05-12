#include "driver_device.hh"

#include <cwchar>

#include "core/error/error_macros.hh"
#include "renderer/types.hh"
#include "vulkan/vulkan_core.h"

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
    _register_requested_device_extension(VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME, false);
    _register_requested_device_extension(VK_EXT_PIPELINE_CREATION_CACHE_CONTROL_EXTENSION_NAME,
                                         false);
    _register_requested_device_extension(VK_EXT_SUBGROUP_SIZE_CONTROL_EXTENSION_NAME, false);
    _register_requested_device_extension(VK_EXT_ASTC_DECODE_MODE_EXTENSION_NAME, false);
    _register_requested_device_extension(VK_EXT_TEXTURE_COMPRESSION_ASTC_HDR_EXTENSION_NAME, false);
    _register_requested_device_extension(VK_QCOM_FRAGMENT_DENSITY_MAP_OFFSET_EXTENSION_NAME, false);

    uint32_t device_extension_count = 0;
    VkResult err = vkEnumerateDeviceExtensionProperties(physical_device, nullptr,
                                                        &device_extension_count, nullptr);
    ERR_FAIL_COND_V(err != VK_SUCCESS, ERR_CANT_CREATE);
    ERR_FAIL_COND_V_MSG(device_extension_count == 0, ERR_CANT_CREATE,
                        "No Vulkan device extensions found");

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
                ERR_FAIL_V_MSG(ERR_CANT_CREATE,
                               ("Required extension" + extension + " not found.").c_str());
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
    VK_DEVICEFEATURE_ENABLE_IF(samplerAnisotropy);
    VK_DEVICEFEATURE_ENABLE_IF(depthClamp);
    VK_DEVICEFEATURE_ENABLE_IF(depthBiasClamp);
    VK_DEVICEFEATURE_ENABLE_IF(sampleRateShading);

    VK_DEVICEFEATURE_ENABLE_IF(geometryShader);
    VK_DEVICEFEATURE_ENABLE_IF(tessellationShader);
    VK_DEVICEFEATURE_ENABLE_IF(fillModeNonSolid);
    VK_DEVICEFEATURE_ENABLE_IF(multiDrawIndirect);
    VK_DEVICEFEATURE_ENABLE_IF(drawIndirectFirstInstance);
    VK_DEVICEFEATURE_ENABLE_IF(dualSrcBlend);

    VK_DEVICEFEATURE_ENABLE_IF(textureCompressionBC);
    VK_DEVICEFEATURE_ENABLE_IF(textureCompressionETC2);
    VK_DEVICEFEATURE_ENABLE_IF(textureCompressionASTC_LDR);

    VK_DEVICEFEATURE_ENABLE_IF(vertexPipelineStoresAndAtomics);
    VK_DEVICEFEATURE_ENABLE_IF(fragmentStoresAndAtomics);
    VK_DEVICEFEATURE_ENABLE_IF(shaderImageGatherExtended);
    VK_DEVICEFEATURE_ENABLE_IF(shaderStorageImageExtendedFormats);
    VK_DEVICEFEATURE_ENABLE_IF(shaderStorageImageReadWithoutFormat);
    VK_DEVICEFEATURE_ENABLE_IF(shaderStorageImageWriteWithoutFormat);

    VK_DEVICEFEATURE_ENABLE_IF(shaderUniformBufferArrayDynamicIndexing);
    VK_DEVICEFEATURE_ENABLE_IF(shaderSampledImageArrayDynamicIndexing);
    VK_DEVICEFEATURE_ENABLE_IF(shaderStorageBufferArrayDynamicIndexing);
    VK_DEVICEFEATURE_ENABLE_IF(shaderStorageImageArrayDynamicIndexing);

    VK_DEVICEFEATURE_ENABLE_IF(shaderInt16);
    VK_DEVICEFEATURE_ENABLE_IF(shaderInt64);
    VK_DEVICEFEATURE_ENABLE_IF(shaderFloat64);
    VK_DEVICEFEATURE_ENABLE_IF(shaderClipDistance);
    VK_DEVICEFEATURE_ENABLE_IF(shaderCullDistance);

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
    device_features_vk_1_2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    device_features_vk_1_2.pNext = next_features;
    next_features = &device_features_vk_1_2;

    VkPhysicalDeviceVulkan11Features device_features_vk_1_1 = {};
    device_features_vk_1_1.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
    device_features_vk_1_1.pNext = next_features;
    next_features = &device_features_vk_1_1;

    VkPhysicalDeviceFragmentShadingRateFeaturesKHR fsr_features = {};
    if (HAS_EXTENSION(VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME)) {
        fsr_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_FEATURES_KHR;
        fsr_features.pNext = next_features;
        next_features = &fsr_features;
    }

    VkPhysicalDeviceFragmentDensityMapFeaturesEXT fdm_features = {};
    if (HAS_EXTENSION(VK_EXT_FRAGMENT_DENSITY_MAP_EXTENSION_NAME)) {
        fdm_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_FEATURES_EXT;
        fdm_features.pNext = next_features;
        next_features = &fdm_features;
    }

    VkPhysicalDeviceFragmentDensityMapOffsetFeaturesQCOM fdmo_features_qcom = {};
    if (HAS_EXTENSION(VK_QCOM_FRAGMENT_DENSITY_MAP_OFFSET_EXTENSION_NAME)) {
        fdmo_features_qcom.sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_OFFSET_FEATURES_QCOM;
        fdmo_features_qcom.pNext = next_features;
        next_features = &fdmo_features_qcom;
    }

    VkPhysicalDevicePipelineCreationCacheControlFeatures pipeline_cache_control_features = {};
    if (HAS_EXTENSION(VK_EXT_PIPELINE_CREATION_CACHE_CONTROL_EXTENSION_NAME)) {
        pipeline_cache_control_features.sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_CREATION_CACHE_CONTROL_FEATURES;
        pipeline_cache_control_features.pNext = next_features;
        next_features = &pipeline_cache_control_features;
    }

    VkPhysicalDeviceFeatures2 device_features_2 = {};
    device_features_2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    device_features_2.pNext = next_features;

    vkGetPhysicalDeviceFeatures2(physical_device, &device_features_2);

    shader_capabilities.shader_float16_is_supported = device_features_vk_1_2.shaderFloat16;
    shader_capabilities.shader_int8_is_supported = device_features_vk_1_2.shaderInt8;
    buffer_device_address_support = device_features_vk_1_2.bufferDeviceAddress;
    vulkan_memory_model_support = device_features_vk_1_2.vulkanMemoryModel;
    vulkan_memory_model_device_scope_support = device_features_vk_1_2.vulkanMemoryModelDeviceScope;

    multiview_capabilities.is_supported = device_features_vk_1_1.multiview;
    multiview_capabilities.geometry_shader_is_supported =
        device_features_vk_1_1.multiviewGeometryShader;
    multiview_capabilities.tessellation_shader_is_supported =
        device_features_vk_1_1.multiviewTessellationShader;

    storage_buffer_capabilities.storage_buffer_16_bit_access_is_supported =
        device_features_vk_1_1.storageBuffer16BitAccess;
    storage_buffer_capabilities.uniform_and_storage_buffer_16_bit_access_is_supported =
        device_features_vk_1_1.uniformAndStorageBuffer16BitAccess;
    storage_buffer_capabilities.storage_push_constant_16_is_supported =
        device_features_vk_1_1.storagePushConstant16;
    storage_buffer_capabilities.storage_input_output_16 =
        device_features_vk_1_1.storageInputOutput16;

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
    if (HAS_EXTENSION(VK_EXT_PIPELINE_CREATION_CACHE_CONTROL_EXTENSION_NAME)) {
        pipeline_cache_control_support =
            pipeline_cache_control_features.pipelineCreationCacheControl;
    }
    if (HAS_EXTENSION(VK_EXT_DEVICE_FAULT_EXTENSION_NAME)) {
        device_fault_support = true;
    }

    void *next_properties = nullptr;

    VkPhysicalDeviceSubgroupProperties subgroup_properties = {};
    subgroup_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES;
    subgroup_properties.pNext = next_properties;
    next_properties = &subgroup_properties;

    VkPhysicalDeviceSubgroupSizeControlProperties subgroup_size_control_properties = {};
    subgroup_capabilities.size_control_is_supported =
        HAS_EXTENSION(VK_EXT_SUBGROUP_SIZE_CONTROL_EXTENSION_NAME);
    if (subgroup_capabilities.size_control_is_supported) {
        subgroup_size_control_properties.sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_PROPERTIES;
        subgroup_size_control_properties.pNext = next_properties;
        next_properties = &subgroup_size_control_properties;
    }

    VkPhysicalDeviceMultiviewProperties multiview_properties = {};
    if (multiview_capabilities.is_supported) {
        multiview_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_PROPERTIES;
        multiview_properties.pNext = next_properties;
        next_properties = &multiview_properties;
    }

    VkPhysicalDeviceFragmentShadingRatePropertiesKHR fsr_properties = {};
    if (fsr_capabilities.attachment_supported) {
        fsr_properties.sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_PROPERTIES_KHR;
        fsr_properties.pNext = next_properties;
        next_properties = &fsr_properties;
    }

    VkPhysicalDeviceFragmentDensityMapPropertiesEXT fdm_properties = {};
    if (fdm_capabilities.attachment_supported) {
        fdm_properties.sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_PROPERTIES_EXT;
        fdm_properties.pNext = next_properties;
        next_properties = &fdm_properties;
    }

    VkPhysicalDeviceFragmentDensityMapOffsetPropertiesQCOM fdmo_properties = {};
    if (fdm_capabilities.offset_supported) {
        fdmo_properties.sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_OFFSET_PROPERTIES_QCOM;
        fdmo_properties.pNext = next_properties;
        next_properties = &fdmo_properties;
    }

    VkPhysicalDeviceProperties2 physical_device_properties_2 = {};
    physical_device_properties_2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    physical_device_properties_2.pNext = next_properties;

    vkGetPhysicalDeviceProperties2(physical_device, &physical_device_properties_2);

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
    VkPhysicalDeviceVulkan12Features features_12 = {};
    features_12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    features_12.pNext = create_info_next;
    features_12.shaderFloat16 = shader_capabilities.shader_float16_is_supported;
    features_12.shaderInt8 = shader_capabilities.shader_int8_is_supported;
    features_12.bufferDeviceAddress = buffer_device_address_support;
    features_12.vulkanMemoryModel = vulkan_memory_model_support;
    features_12.vulkanMemoryModelDeviceScope = vulkan_memory_model_device_scope_support;
    create_info_next = &features_12;

    VkPhysicalDeviceVulkan11Features features_11 = {};
    features_11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
    features_11.pNext = create_info_next;
    features_11.storageBuffer16BitAccess =
        storage_buffer_capabilities.storage_buffer_16_bit_access_is_supported;
    features_11.uniformAndStorageBuffer16BitAccess =
        storage_buffer_capabilities.uniform_and_storage_buffer_16_bit_access_is_supported;
    features_11.storagePushConstant16 =
        storage_buffer_capabilities.storage_push_constant_16_is_supported;
    features_11.storageInputOutput16 = storage_buffer_capabilities.storage_input_output_16;
    features_11.multiview = multiview_capabilities.is_supported;
    features_11.multiviewGeometryShader = multiview_capabilities.geometry_shader_is_supported;
    features_11.multiviewTessellationShader =
        multiview_capabilities.tessellation_shader_is_supported;
    create_info_next = &features_11;

    // Optional Extension Chains
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
        device_fault_features.deviceFault = VK_TRUE;
        device_fault_features.deviceFaultVendorBinary = VK_TRUE;
        create_info_next = &device_fault_features;
    }

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
    const bool use_1_3_features =
        VulkanApiVersion(physical_device_properties.apiVersion).supports_vulkan_1_3();
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

/* buffer */
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

/* texture */
Ref<Texture> RenderingDeviceDriver::texture_create(const TextureFormat &p_format,
                                                   const TextureView &p_view) {
    Ref<Texture> texture = new Texture();

    VkImageCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;

    create_info.imageType = p_format.texture_type;
    create_info.format = p_format.format;

    create_info.extent.width = p_format.width;
    create_info.extent.height = p_format.height;
    create_info.extent.depth = p_format.depth;

    create_info.mipLevels = p_format.mipmaps;
    create_info.arrayLayers = p_format.array_layers;

    create_info.samples = p_format.samples;
    create_info.tiling = VK_IMAGE_TILING_LINEAR;

    create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkResult err = vkCreateImage(vk_device, &create_info, nullptr, &texture->vk_image);
    ERR_FAIL_COND_V_MSG(err != VK_SUCCESS, nullptr, "Failed to create image");

    // image view
    VkImageViewCreateInfo image_view_create_info = {};
    image_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    image_view_create_info.image = texture->vk_image;
    image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    image_view_create_info.format = p_format.format;
    image_view_create_info.components.r = p_view.swizzle_r;
    image_view_create_info.components.g = p_view.swizzle_g;
    image_view_create_info.components.b = p_view.swizzle_b;
    image_view_create_info.components.a = p_view.swizzle_a;
    image_view_create_info.subresourceRange.levelCount = create_info.mipLevels;
    image_view_create_info.subresourceRange.layerCount = create_info.arrayLayers;
    image_view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

    err = vkCreateImageView(vk_device, &image_view_create_info, nullptr, &texture->vk_image_view);
    ERR_FAIL_COND_V_MSG(err != VK_SUCCESS, nullptr, "Failed to create image views");

    return texture;
}

void RenderingDeviceDriver::texture_free(Ref<Texture> p_texture) {
    vkDestroyImageView(vk_device, p_texture->vk_image_view, nullptr);
    if (p_texture->allocation.handle) {
        vkDestroyImage(vk_device, p_texture->vk_image, nullptr);
        vmaFreeMemory(allocator, p_texture->allocation.handle);
    }
}

/* sampler */
Ref<Sampler> RenderingDeviceDriver::sampler_create(const SamplerState &p_state) {
    VkSamplerCreateInfo sampler_ci = {};
    sampler_ci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_ci.pNext = nullptr;
    sampler_ci.flags = 0;
    sampler_ci.magFilter = p_state.mag_filter;
    sampler_ci.minFilter = p_state.min_filter;
    sampler_ci.mipmapMode = p_state.mip_filter;
    sampler_ci.addressModeU = p_state.repeat_u;
    sampler_ci.addressModeV = p_state.repeat_v;
    sampler_ci.addressModeW = p_state.repeat_w;
    sampler_ci.mipLodBias = p_state.lod_bias;
    sampler_ci.anisotropyEnable =
        p_state.use_anisotropy && (physical_device_features.samplerAnisotropy == VK_TRUE);
    sampler_ci.maxAnisotropy = p_state.anisotropy_max;
    sampler_ci.compareEnable = p_state.compare_op;
    sampler_ci.minLod = p_state.min_lod;
    sampler_ci.maxLod = p_state.max_lod;
    sampler_ci.borderColor = p_state.border_color;
    sampler_ci.unnormalizedCoordinates = p_state.unnormalized_uvw;

    Ref<Sampler> sampler = new Sampler();
    VkResult res = vkCreateSampler(vk_device, &sampler_ci, nullptr, &sampler->handle);
    ERR_FAIL_COND_V_MSG(res != VK_SUCCESS, nullptr, "Couldn't create Vulkan sampler");

    return sampler;
}

/* fence */
Ref<Fence> RenderingDeviceDriver::fence_create() {
    Ref<Fence> fence = new Fence();

    VkFenceCreateInfo fence_create_info = {};
    fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_create_info.pNext = nullptr;
    fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VkResult result = vkCreateFence(vk_device, &fence_create_info, nullptr, &fence->vk_fence);

    ERR_FAIL_COND_V_MSG(result != VK_SUCCESS, nullptr, "Couldn't create vulkan fence");

    return fence;
}

Error RenderingDeviceDriver::fence_wait(Ref<Fence> p_fence) {
    VkResult fence_status = vkGetFenceStatus(vk_device, p_fence->vk_fence);
    if (fence_status == VK_NOT_READY) {
        VkResult err = vkWaitForFences(vk_device, 1, &p_fence->vk_fence, VK_TRUE, UINT64_MAX);
        ERR_FAIL_COND_V_MSG(err != VK_SUCCESS, FAILED, "Couldn't wait for Vulkan fence");
    }

    VkResult err = vkResetFences(vk_device, 1, &p_fence->vk_fence);
    ERR_FAIL_COND_V_MSG(err != VK_SUCCESS, FAILED, "Couldn't reset Vulkan fence");

    return OK;
}

void RenderingDeviceDriver::fence_free(Ref<Fence> p_fence) {
    vkDestroyFence(vk_device, p_fence->vk_fence, nullptr);
}

void RenderingDeviceDriver::sampler_free(Ref<Sampler> p_sampler) {
    vkDestroySampler(vk_device, p_sampler->handle, nullptr);
}

RenderingDeviceDriver::RenderingDeviceDriver(RenderingDriverContext *p_context_driver)
    : context_driver(p_context_driver) {}

RenderingDeviceDriver::~RenderingDeviceDriver() {}
