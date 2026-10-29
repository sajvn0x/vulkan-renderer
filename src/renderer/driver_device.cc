#include "driver_device.hh"

#include <algorithm>

#include "core/error/error_macros.hh"
#include "renderer/types.hh"
#include "shader_container.hh"
#include "vulkan/vulkan_core.h"

static const uint32_t MAX_DYNAMIC_BUFFERS = 8u;

Error RDD::initialize(uint32_t p_device_index, uint32_t p_frame_count) {
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

/* fence */
Ref<Fence> RDD::fence_create() {
    Ref<Fence> fence = new Fence();

    VkFenceCreateInfo fence_create_info = {};
    fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_create_info.pNext = nullptr;
    fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VkResult result = vkCreateFence(vk_device, &fence_create_info, nullptr, &fence->vk_fence);

    ERR_FAIL_COND_V_MSG(result != VK_SUCCESS, nullptr, "Couldn't create vulkan fence");

    return fence;
}

Error RDD::fence_wait(Ref<Fence> p_fence) {
    VkResult fence_status = vkGetFenceStatus(vk_device, p_fence->vk_fence);
    if (fence_status == VK_NOT_READY) {
        VkResult err = vkWaitForFences(vk_device, 1, &p_fence->vk_fence, VK_TRUE, UINT64_MAX);
        ERR_FAIL_COND_V_MSG(err != VK_SUCCESS, FAILED, "Couldn't wait for Vulkan fence");
    }

    VkResult err = vkResetFences(vk_device, 1, &p_fence->vk_fence);
    ERR_FAIL_COND_V_MSG(err != VK_SUCCESS, FAILED, "Couldn't reset Vulkan fence");

    return OK;
}

void RDD::fence_free(Ref<Fence> p_fence) { vkDestroyFence(vk_device, p_fence->vk_fence, nullptr); }

/* semaphore */
Ref<Semaphore> RDD::semaphore_create() {
    Ref<Semaphore> semaphore = new Semaphore();

    VkSemaphoreCreateInfo semaphore_create_info = {};
    semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkResult err =
        vkCreateSemaphore(vk_device, &semaphore_create_info, nullptr, &semaphore->vk_semaphore);
    ERR_FAIL_COND_V_MSG(err != VK_SUCCESS, nullptr, "Couldn't create Vulkan semaphore");

    return semaphore;
}

void RDD::semaphore_free(Ref<Semaphore> p_semaphore) {
    vkDestroySemaphore(vk_device, p_semaphore->vk_semaphore, nullptr);
}

void RDD::command_pipeline_barrier(RDD::CommandBufferID p_cmd_buffer,
                                   VkMemoryBarrier2 &p_memory_barrier,
                                   Vector<VkBufferMemoryBarrier2> &p_buffer_barriers,
                                   Vector<VkImageMemoryBarrier2> &p_image_barriers) {
    CommandBuffer *cmd_buffer = (CommandBuffer *)p_cmd_buffer.id;

    VkDependencyInfo dependency_info = {};
    dependency_info.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    dependency_info.memoryBarrierCount = 1;
    dependency_info.pMemoryBarriers = &p_memory_barrier;
    dependency_info.bufferMemoryBarrierCount = p_buffer_barriers.size();
    dependency_info.pBufferMemoryBarriers = p_buffer_barriers.data();
    dependency_info.imageMemoryBarrierCount = p_image_barriers.size();
    dependency_info.pImageMemoryBarriers = p_image_barriers.data();

    vkCmdPipelineBarrier2(cmd_buffer->vk_command_buffer, &dependency_info);
}

static const uint32_t SMALL_ALLOCATION_MAX_SIZE = 4096;

VmaPool RDD::_find_or_create_small_allocs_pool(uint32_t p_mem_type_index) {
    if (small_allocs_pools.count(p_mem_type_index) > 0) {
        return small_allocs_pools[p_mem_type_index];
    }

    VmaPoolCreateInfo pci = {};
    pci.memoryTypeIndex = p_mem_type_index;
    pci.flags = 0;
    pci.blockSize = 0;
    pci.minBlockCount = 0;
    pci.maxBlockCount = SIZE_MAX;
    pci.priority = 0.5f;
    pci.minAllocationAlignment = 0;
    pci.pMemoryAllocateNext = nullptr;
    VmaPool pool = VK_NULL_HANDLE;
    VkResult res = vmaCreatePool(allocator, &pci, &pool);
    small_allocs_pools[p_mem_type_index] = pool;
    ERR_FAIL_COND_V_MSG(res, pool, "Couldn't create Vulkan memory allocator pool");
    return pool;
}

void RDD::_register_requested_device_extension(const String &p_extension_name, bool p_required) {
    requested_device_extensions[p_extension_name] = p_required;
}

Error RDD::_initialize_device_extensions() {
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

Error RDD::_check_device_features() {
    vkGetPhysicalDeviceFeatures(physical_device, &physical_device_features);

    physical_device_features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;

    physical_device_features11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
    physical_device_features11.pNext = &physical_device_features13;

    physical_device_features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    physical_device_features2.pNext = &physical_device_features11;

    vkGetPhysicalDeviceFeatures2(physical_device, &physical_device_features2);

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
    if (physical_device_features13.dynamicRendering)
        requested_device_features13.dynamicRendering = VK_TRUE;

    return OK;
}

Error RDD::_check_device_capabilities() {
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

Error RDD::_add_queue_create_info(Vector<VkDeviceQueueCreateInfo> &r_queue_create_info) {
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

Error RDD::_initialize_device(const Vector<VkDeviceQueueCreateInfo> &p_queue_create_info) {
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

    VkPhysicalDeviceFaultFeaturesEXT device_fault_features = {};
    if (device_fault_support) {
        device_fault_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FAULT_FEATURES_EXT;
        device_fault_features.pNext = create_info_next;
        device_fault_features.deviceFault = VK_TRUE;
        device_fault_features.deviceFaultVendorBinary = VK_TRUE;
        create_info_next = &device_fault_features;
    }

    requested_device_features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    requested_device_features13.pNext = create_info_next;
    create_info_next = &requested_device_features13;

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

Error RDD::_initialize_allocator() {
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
Ref<Buffer> RDD::buffer_create(uint64_t p_size, VkBufferUsageFlags p_usage,
                               MemoryAllocationType p_allocation_type) {
#define STEPIFY(m_number, m_alignment) \
    ((((m_number) + ((m_alignment) - 1)) / (m_alignment)) * (m_alignment))

    Ref<Buffer> buffer = new Buffer();

    uint32_t alignment = 16u;
    if (p_usage & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT) {
        alignment = std::max<uint64_t>(
            alignment, physical_device_properties.limits.minUniformBufferOffsetAlignment);
    }
    if (p_usage & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT) {
        alignment = std::max<uint64_t>(
            {alignment, physical_device_properties.limits.minStorageBufferOffsetAlignment});
    }
    p_size = STEPIFY(p_size, alignment);
    const size_t original_size = p_size;

    VkBufferCreateInfo buffer_info = {};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = p_size;
    buffer_info.usage = p_usage;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaMemoryUsage vma_usage = VMA_MEMORY_USAGE_UNKNOWN;
    uint32_t vma_flags_to_remove = 0;

    VmaAllocationCreateInfo alloc_info = {};
    switch (p_allocation_type) {
        case MEMORY_ALLOCATION_TYPE_CPU: {
            bool is_src = p_usage & VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            bool is_dst = p_usage & VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            if (is_src && !is_dst) {
                alloc_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
                alloc_info.preferredFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
                vma_flags_to_remove |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
            }

            if (is_dst && !is_src) {
                alloc_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
                alloc_info.preferredFlags |= VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
                vma_flags_to_remove |= VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
            }
            vma_usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
            alloc_info.requiredFlags =
                (VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        } break;
        case MEMORY_ALLOCATION_TYPE_GPU: {
            alloc_info.preferredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
            if (p_size <= SMALL_ALLOCATION_MAX_SIZE) {
                uint32_t mem_type_index = 0;
                vmaFindMemoryTypeIndexForBufferInfo(allocator, &buffer_info, &alloc_info,
                                                    &mem_type_index);
                alloc_info.pool = _find_or_create_small_allocs_pool(mem_type_index);
            }
        } break;
        case MEMORY_ALLOCATION_TYPE_GPU_MAPPABLE: {
            vma_usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
            alloc_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
            alloc_info.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
                                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                       VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        } break;
    }

    VkBuffer vk_buffer = VK_NULL_HANDLE;
    VmaAllocation allocation = nullptr;
    VmaAllocationInfo vma_alloc_info = {};

    VkResult err = vkCreateBuffer(vk_device, &buffer_info, nullptr, &vk_buffer);
    ERR_FAIL_COND_V_MSG(err, nullptr, "Can't create buffer");
    err =
        vmaAllocateMemoryForBuffer(allocator, vk_buffer, &alloc_info, &allocation, &vma_alloc_info);
    ERR_FAIL_COND_V_MSG(err, nullptr, "Can't allocate memory for buffer");
    err = vmaBindBufferMemory2(allocator, allocation, 0, vk_buffer, nullptr);
    ERR_FAIL_COND_V_MSG(err, nullptr, "Can't bind memory to buffer");

    buffer->vk_buffer = vk_buffer;
    buffer->allocation.handle = allocation;
    buffer->allocation.size = vma_alloc_info.size;
    buffer->size = original_size;

    return buffer;
}

bool RDD::buffer_set_texel_format(Ref<Buffer> p_buffer, VkFormat p_format) {
    DEV_ASSERT(!p_buffer->vk_view);

    VkBufferViewCreateInfo view_create_info = {};
    view_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
    view_create_info.buffer = p_buffer->vk_buffer;
    view_create_info.format = p_format;
    view_create_info.range = p_buffer->allocation.size;

    VkResult res = vkCreateBufferView(vk_device, &view_create_info, nullptr, &p_buffer->vk_view);
    ERR_FAIL_COND_V_MSG(res, false, "Couldn't create Vulkan buffer view");
    return true;
}

void RDD::buffer_free(Ref<Buffer> p_buffer, VkFormat p_format) {
    if (p_buffer->vk_view) {
        vkDestroyBufferView(vk_device, p_buffer->vk_view, nullptr);
    }
    if (p_buffer->is_dynamic()) {
        vmaUnmapMemory(allocator, p_buffer->allocation.handle);
    }

    vmaDestroyBuffer(allocator, p_buffer->vk_buffer, p_buffer->allocation.handle);
}

uint64_t RDD::buffer_get_allocation_size(Ref<Buffer> p_buffer) { return p_buffer->allocation.size; }

uint8_t *RDD::buffer_map(Ref<Buffer> p_buffer) {
    void *data_ptr = nullptr;

    VkResult err = vmaMapMemory(allocator, p_buffer->allocation.handle, &data_ptr);
    ERR_FAIL_COND_V_MSG(err, nullptr, "Couldn't map buffer memory");

    return (uint8_t *)data_ptr;
}

void RDD::buffer_unmap(Ref<Buffer> p_buffer) {
    vmaUnmapMemory(allocator, p_buffer->allocation.handle);
}

uint64_t RDD::buffer_get_device_address(Ref<Buffer> p_buffer) {
    VkBufferDeviceAddressInfo address_info = {};
    address_info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    address_info.pNext = nullptr;
    address_info.buffer = p_buffer->vk_buffer;
    return vkGetBufferDeviceAddress(vk_device, &address_info);
}

/* texture */
VkSampleCountFlagBits RDD::_ensure_supported_sample_count(
    VkSampleCountFlagBits p_requested_sample_count) {
    VkSampleCountFlags sample_count_flags =
        (physical_device_properties.limits.framebufferColorSampleCounts &
         physical_device_properties.limits.framebufferDepthSampleCounts);

    if (sample_count_flags & p_requested_sample_count) {
        return p_requested_sample_count;
    } else {
        VkSampleCountFlagBits sample_count = p_requested_sample_count;
        while (sample_count > VK_SAMPLE_COUNT_1_BIT) {
            if (sample_count_flags & sample_count) {
                return sample_count;
            }
            sample_count = (VkSampleCountFlagBits)(sample_count >> 1);
        }
    }
    return VK_SAMPLE_COUNT_1_BIT;
}

Ref<Texture> RDD::texture_create(const TextureFormat &p_format, const TextureView &p_view) {
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
    image_view_create_info.viewType = p_view.image_view;
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

void RDD::texture_free(Ref<Texture> p_texture) {
    vkDestroyImageView(vk_device, p_texture->vk_image_view, nullptr);
    if (p_texture->allocation.handle) {
        vkDestroyImage(vk_device, p_texture->vk_image, nullptr);
        vmaFreeMemory(allocator, p_texture->allocation.handle);
    }
}

/* sampler */
Ref<Sampler> RDD::sampler_create(const SamplerState &p_state) {
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

void RDD::sampler_free(Ref<Sampler> p_sampler) {
    vkDestroySampler(vk_device, p_sampler->handle, nullptr);
}

bool RDD::sampler_is_format_supported_for_filter(VkFormat p_format, VkFilter p_filter) {
    switch (p_filter) {
        case VK_FILTER_NEAREST: {
            return true;
        }
        case VK_FILTER_LINEAR: {
            VkFormatProperties properties = {};
            vkGetPhysicalDeviceFormatProperties(physical_device, p_format, &properties);
            return (properties.optimalTilingFeatures &
                    VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT);
        }
        default:
            break;
    }
    return true;
}

// render pass
Ref<RenderPass> RDD::render_pass_create(Vector<Attachment> &p_attachments,
                                        Vector<Subpass> &p_subpasses,
                                        Vector<SubpassDependency> &p_subpass_dependencies) {
    Ref<RenderPass> render_pass = new RenderPass();

    Vector<VkAttachmentDescription2> vk_attachments(p_attachments.size());

    for (uint32_t i = 0; i < p_attachments.size(); i++) {
        vk_attachments[i] = {};
        vk_attachments[i].sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2;
        vk_attachments[i].pNext = nullptr;
        vk_attachments[i].flags = 0;
        vk_attachments[i].format = p_attachments[i].format;
        vk_attachments[i].samples = p_attachments[i].samples;
        vk_attachments[i].loadOp = p_attachments[i].load_op;
        vk_attachments[i].storeOp = p_attachments[i].store_op;
        vk_attachments[i].stencilLoadOp = p_attachments[i].stencil_load_op;
        vk_attachments[i].stencilStoreOp = p_attachments[i].stencil_store_op;
        vk_attachments[i].initialLayout = p_attachments[i].initial_layout;
        vk_attachments[i].finalLayout = p_attachments[i].final_layout;
    }

    Vector<VkSubpassDescription2> vk_subpasses(p_subpasses.size());
    for (uint32_t i = 0; i < p_subpasses.size(); i++) {
        vk_subpasses[i] = {};
        vk_subpasses[i].sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2;
        vk_subpasses[i].pNext = nullptr;
        vk_subpasses[i].flags = 0;
        vk_subpasses[i].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        vk_subpasses[i].viewMask = 0;
        vk_subpasses[i].inputAttachmentCount = p_subpasses[i].input_references.size();
        vk_subpasses[i].pInputAttachments = p_subpasses[i].input_references.data();
        vk_subpasses[i].colorAttachmentCount = p_subpasses[i].color_references.size();
        vk_subpasses[i].pColorAttachments = p_subpasses[i].color_references.data();
        vk_subpasses[i].pResolveAttachments = p_subpasses[i].resolve_references.data();
        vk_subpasses[i].pDepthStencilAttachment = &p_subpasses[i].depth_stencil_reference;
        vk_subpasses[i].preserveAttachmentCount = p_subpasses[i].preserve_attachments.size();
        vk_subpasses[i].pPreserveAttachments = p_subpasses[i].preserve_attachments.data();
    }

    Vector<VkSubpassDependency2> vk_subpass_dependencies(p_subpass_dependencies.size());
    for (uint32_t i = 0; i < p_subpass_dependencies.size(); i++) {
        vk_subpass_dependencies[i] = {};
        vk_subpass_dependencies[i].sType = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2;
        vk_subpass_dependencies[i].pNext = nullptr;
        vk_subpass_dependencies[i].srcSubpass = p_subpass_dependencies[i].src_subpass;
        vk_subpass_dependencies[i].dstSubpass = p_subpass_dependencies[i].dst_subpass;
        vk_subpass_dependencies[i].srcStageMask = p_subpass_dependencies[i].src_stages;
        vk_subpass_dependencies[i].dstStageMask = p_subpass_dependencies[i].dst_stages;
        vk_subpass_dependencies[i].srcAccessMask = p_subpass_dependencies[i].src_access;
        vk_subpass_dependencies[i].dstAccessMask = p_subpass_dependencies[i].dst_access;
        vk_subpass_dependencies[i].dependencyFlags = 0;
        vk_subpass_dependencies[i].viewOffset = 0;
    }

    VkRenderPassCreateInfo2 create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2;
    create_info.pNext = nullptr;
    create_info.flags = 0;
    create_info.attachmentCount = vk_attachments.size();
    create_info.pAttachments = vk_attachments.data();
    create_info.subpassCount = vk_subpasses.size();
    create_info.pSubpasses = vk_subpasses.data();
    create_info.dependencyCount = vk_subpass_dependencies.size();
    create_info.pDependencies = vk_subpass_dependencies.data();
    create_info.correlatedViewMaskCount = 0;
    create_info.pCorrelatedViewMasks = nullptr;

    vkCreateRenderPass2(vk_device, &create_info, nullptr, &render_pass->vk_render_pass);

    return render_pass;
}

void RDD::render_pass_free(Ref<RenderPass> p_render_pass) {
    vkDestroyRenderPass(vk_device, p_render_pass->vk_render_pass, nullptr);
}

void RDD::command_render_begin_rendering(Ref<CommandBuffer> p_cmd_buffer, VkRect2D p_render_area) {
    VkRenderingAttachmentInfo color_attachment_info = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .pNext = nullptr,
        .imageView = nullptr,
        .imageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .resolveMode = VK_RESOLVE_MODE_NONE,
        .resolveImageView = nullptr,
        .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_NONE,
        .clearValue = (VkClearValue){},
    };

    VkImageView color_image_view = VK_NULL_HANDLE;
    VkImageViewCreateInfo color_image_view_ci = {};
    color_image_view_ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;

    vkCreateImageView(vk_device, &color_image_view_ci, nullptr, &color_image_view);

    VkRenderingAttachmentInfo depth_attachment_info = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .pNext = nullptr,
        .imageView = nullptr,
        .imageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .resolveMode = VK_RESOLVE_MODE_NONE,
        .resolveImageView = nullptr,
        .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_NONE,
        .clearValue = (VkClearValue){},
    };
    VkRenderingAttachmentInfo stencil_attachment_info = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .pNext = nullptr,
        .imageView = nullptr,
        .imageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .resolveMode = VK_RESOLVE_MODE_NONE,
        .resolveImageView = nullptr,
        .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_NONE,
        .clearValue = (VkClearValue){},
    };

    VkRenderingInfo rendering_info = {};
    rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    rendering_info.flags = 0;
    rendering_info.renderArea = p_render_area, rendering_info.layerCount = 1,
    rendering_info.viewMask = 0, rendering_info.colorAttachmentCount = 1,
    rendering_info.pColorAttachments = &color_attachment_info,
    rendering_info.pDepthAttachment = &depth_attachment_info,
    rendering_info.pStencilAttachment = &stencil_attachment_info,

    vkCmdBeginRendering(p_cmd_buffer->vk_command_buffer, &rendering_info);
}

void RDD::command_render_end_rendering(Ref<CommandBuffer> p_cmd_buffer) {
    vkCmdEndRendering(p_cmd_buffer->vk_command_buffer);
}

void RDD::command_begin_render_pass(Ref<CommandBuffer> p_cmd_buffer, VkRenderPass p_render_pass,
                                    Ref<Framebuffer> p_framebuffer,
                                    VkCommandBufferLevel p_cmd_level, const VkRect2D &p_rect,
                                    Vector<VkClearValue> p_clear_values) {
    VkRenderPassBeginInfo render_pass_begin = {};
    render_pass_begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_begin.pNext = nullptr;
    render_pass_begin.renderPass = p_render_pass;
    render_pass_begin.framebuffer = p_framebuffer->vk_framebuffer;
    render_pass_begin.renderArea = p_rect;
    render_pass_begin.clearValueCount = p_clear_values.size();
    render_pass_begin.pClearValues = p_clear_values.data();

    VkSubpassContents vk_subpass_contents = p_cmd_level == VK_COMMAND_BUFFER_LEVEL_PRIMARY
                                                ? VK_SUBPASS_CONTENTS_INLINE
                                                : VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS;
    vkCmdBeginRenderPass(p_cmd_buffer->vk_command_buffer, &render_pass_begin, vk_subpass_contents);
}

void RDD::command_end_render_pass(Ref<CommandBuffer> p_cmd_buffer) {
    DEV_ASSERT(p_cmd_buffer->active_framebuffer != nullptr);

    vkCmdEndRenderPass(p_cmd_buffer->vk_command_buffer);
    p_cmd_buffer->active_framebuffer = nullptr;
}

void RDD::command_next_render_subpass(Ref<CommandBuffer> p_cmd_buffer,
                                      VkCommandBufferLevel p_cmd_level) {
    VkSubpassContents vk_subpass_contents = p_cmd_level == VK_COMMAND_BUFFER_LEVEL_PRIMARY
                                                ? VK_SUBPASS_CONTENTS_INLINE
                                                : VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS;

    vkCmdNextSubpass(p_cmd_buffer->vk_command_buffer, vk_subpass_contents);
}

void RDD::command_render_set_viewport(Ref<CommandBuffer> p_cmd_buffer,
                                      Vector<VkRect2D> p_viewports) {
    Vector<VkViewport> vk_viewports(p_viewports.size());
    for (uint32_t i = 0; i < p_viewports.size(); i++) {
        vk_viewports[i] = {};
        vk_viewports[i].x = p_viewports[i].offset.x;
        vk_viewports[i].y = p_viewports[i].offset.y;
        vk_viewports[i].width = p_viewports[i].extent.width;
        vk_viewports[i].height = p_viewports[i].extent.height;
        vk_viewports[i].minDepth = 0.f;
        vk_viewports[i].maxDepth = 1.f;
    }

    vkCmdSetViewport(p_cmd_buffer->vk_command_buffer, 0, p_viewports.size(), vk_viewports.data());
}

void RDD::command_render_set_scissor(Ref<CommandBuffer> p_cmd_buffer, Vector<VkRect2D> p_scissors) {
    vkCmdSetScissor(p_cmd_buffer->vk_command_buffer, 0, p_scissors.size(), p_scissors.data());
}

void RDD::command_render_clear_attachments(Ref<CommandBuffer> p_cmd_buffer,
                                           Vector<VkClearAttachment> p_attachment_clears,
                                           Vector<VkRect2D> p_rects) {
    Vector<VkClearAttachment> vk_clear_attachments(p_attachment_clears.size());

    Vector<VkClearRect> vk_rects(p_rects.size());
    for (uint32_t i = 0; i < p_rects.size(); i++) {
        vk_rects[i] = {};
        vk_rects[i].rect = p_rects[i];
        vk_rects[i].baseArrayLayer = 0;
        vk_rects[i].layerCount = 1;
    }

    vkCmdClearAttachments(p_cmd_buffer->vk_command_buffer, p_attachment_clears.size(),
                          p_attachment_clears.data(), vk_rects.size(), vk_rects.data());
}

Ref<Framebuffer> RDD::framebuffer_create(Ref<RenderPass> p_render_pass,
                                         Vector<Ref<Texture>> p_attachments, uint32_t p_width,
                                         uint32_t p_height) {
    Ref<Framebuffer> framebuffer = new Framebuffer();

    Vector<VkImageView> vk_image_views(p_attachments.size());

    for (uint32_t i = 0; i < p_attachments.size(); ++i) {
        vk_image_views[i] = p_attachments[i]->vk_image_view;
    }

    VkFramebufferCreateInfo framebuffer_create_info = {};
    framebuffer_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebuffer_create_info.renderPass = p_render_pass->vk_render_pass;
    framebuffer_create_info.attachmentCount = p_attachments.size();
    framebuffer_create_info.pAttachments = vk_image_views.data();
    framebuffer_create_info.width = p_width;
    framebuffer_create_info.height = p_height;
    framebuffer_create_info.layers = 1;

    VkResult err = vkCreateFramebuffer(vk_device, &framebuffer_create_info, nullptr,
                                       &framebuffer->vk_framebuffer);
    ERR_FAIL_COND_V_MSG(err, nullptr, "Couldn't create Vulkan framebuffer");

    return framebuffer;
}

void RDD::framebuffer_free(Ref<Framebuffer> p_framebuffer) {
    vkDestroyFramebuffer(vk_device, p_framebuffer->vk_framebuffer, nullptr);
}

uint32_t RDD::command_queue_family_get() { return 0; }

Ref<CommandQueue> RDD::command_queue_create(int p_cmd_queue_family, uint32_t p_image_count) {
    Ref<CommandQueue> cmd_queue = new CommandQueue();

    cmd_queue->queue_family = p_cmd_queue_family;

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        Ref<Semaphore> acquire_semaphore = semaphore_create();
        ERR_FAIL_COND_V_MSG(acquire_semaphore == nullptr, nullptr, "Failed to create semaphore");
        cmd_queue->acquire_semaphores.push_back(acquire_semaphore);
    }

    for (uint32_t i = 0; i < p_image_count; ++i) {
        Ref<Semaphore> submit_semaphore = semaphore_create();
        ERR_FAIL_COND_V_MSG(submit_semaphore == nullptr, nullptr, "Failed to create semaphore");
        cmd_queue->submit_semaphores.push_back(submit_semaphore);
    }

    return cmd_queue;
}

Error RDD::command_queue_execute_and_present(Ref<CommandQueue> p_cmd_queue, Ref<Fence> p_fence,
                                             Ref<Swapchain> p_swapchain,
                                             Ref<CommandBuffer> p_cmd_buffer,
                                             uint32_t p_frame_index) {
    // TODO: choose the value queue
    VkQueue queue = queue_families[p_cmd_queue->queue_family][p_cmd_queue->queue_index].queue;
    VkFence fence = p_fence->vk_fence;
    VkSemaphore acquire_semaphore = p_cmd_queue->acquire_semaphores[p_frame_index]->vk_semaphore;
    VkSemaphore submit_semaphore =
        p_cmd_queue->submit_semaphores[p_swapchain->image_index]->vk_semaphore;

    VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &acquire_semaphore;
    submit_info.pWaitDstStageMask = wait_stages;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &p_cmd_buffer->vk_command_buffer;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &submit_semaphore;

    VkResult result = vkQueueSubmit(queue, 1, &submit_info, fence);
    ERR_FAIL_COND_V(result, FAILED);

    VkPresentInfoKHR present_info = {};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &submit_semaphore;
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &p_swapchain->vk_swapchain;
    present_info.pImageIndices = &p_swapchain->image_index;

    vkQueuePresentKHR(queue, &present_info);

    return OK;
}

void command_queue_free(Ref<CommandQueue> p_cmd_queue) {}

/* commands */
// command pool
Ref<CommandPool> RDD::command_pool_create(uint32_t queue_family_index,
                                          VkCommandBufferLevel cmd_buffer_level) {
    Ref<CommandPool> cmd_pool = new CommandPool();

    VkCommandPoolCreateInfo cmd_pool_info = {};
    cmd_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmd_pool_info.queueFamilyIndex = queue_family_index;
    // enable command buffer reset
    cmd_pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    VkResult res =
        vkCreateCommandPool(vk_device, &cmd_pool_info, nullptr, &cmd_pool->vk_command_pool);
    ERR_FAIL_COND_V_MSG(res, nullptr, "Couldn't create Vulkan command pool");

    return cmd_pool;
}

bool RDD::command_pool_reset(Ref<CommandPool> p_cmd_pool) {
    VkResult err = vkResetCommandPool(vk_device, p_cmd_pool->vk_command_pool, 0);
    ERR_FAIL_COND_V_MSG(err, false, "Couldn't reset Vulkan command pool");

    return true;
}

void RDD::command_pool_free(Ref<CommandPool> p_cmd_pool) {
    vkDestroyCommandPool(vk_device, p_cmd_pool->vk_command_pool, nullptr);
}

// command buffer
Ref<CommandBuffer> RDD::command_buffer_create(Ref<CommandPool> p_cmd_pool) {
    Ref<CommandBuffer> command_buffer = new CommandBuffer();

    VkCommandBufferAllocateInfo cmd_buf_info = {};
    cmd_buf_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmd_buf_info.commandPool = p_cmd_pool->vk_command_pool;
    cmd_buf_info.commandBufferCount = 1;

    if (p_cmd_pool->command_buffer_level == VK_COMMAND_BUFFER_LEVEL_PRIMARY) {
        cmd_buf_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    } else {
        cmd_buf_info.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
    };

    VkResult err =
        vkAllocateCommandBuffers(vk_device, &cmd_buf_info, &command_buffer->vk_command_buffer);
    ERR_FAIL_COND_V_MSG(err, nullptr, "Couldn't allocate Vulkan command buffer");

    return command_buffer;
}

bool RDD::command_buffer_begin(Ref<CommandBuffer> p_cmd_buffer) {
    VkCommandBufferBeginInfo cmd_buf_begin_info = {};
    cmd_buf_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmd_buf_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    VkResult err = vkBeginCommandBuffer(p_cmd_buffer->vk_command_buffer, &cmd_buf_begin_info);
    ERR_FAIL_COND_V_MSG(err, false, "Couldn't begin Vulkan command buffer");

    return true;
}

bool RDD::command_buffer_begin_secondary(Ref<CommandBuffer> p_cmd_buffer) {
    VkCommandBufferBeginInfo cmd_buf_begin_info = {};
    cmd_buf_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmd_buf_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT |
                               VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;

    VkResult err = vkBeginCommandBuffer(p_cmd_buffer->vk_command_buffer, &cmd_buf_begin_info);
    ERR_FAIL_COND_V_MSG(err, false, "Couldn't begin Vulkan command buffer");

    return true;
}

void RDD::command_buffer_end(Ref<CommandBuffer> p_cmd_buffer) {
    vkEndCommandBuffer(p_cmd_buffer->vk_command_buffer);
}

void RDD::command_buffer_execute_secondary(Ref<CommandBuffer> p_cmd_buffer,
                                           Vector<Ref<CommandBuffer>> p_secondary_cmd_buffers) {
    Vector<VkCommandBuffer> secondary_command_buffers;
    secondary_command_buffers.resize(p_secondary_cmd_buffers.size());

    for (uint32_t i = 0; i < p_secondary_cmd_buffers.size(); i++) {
        secondary_command_buffers[i] = p_secondary_cmd_buffers[i]->vk_command_buffer;
    }

    vkCmdExecuteCommands(p_cmd_buffer->vk_command_buffer, p_secondary_cmd_buffers.size(),
                         secondary_command_buffers.data());
}

Ref<RenderPipeline> RDD::render_pipeline_create(
    String program_name, VkPrimitiveTopology p_render_primitive,
    VkPipelineVertexInputStateCreateInfo vertex_input,
    PipelineRasterizationState p_rasterization_state, PipelineMultisampleState p_multisample_state,
    PipelineDepthStencilState p_depth_stencil_state, PipelineColorBlendState p_blend_state,
    Vector<int32_t> p_color_attachments, Vector<VkDynamicState> p_dynamic_state,
    VkRenderPass p_render_pass, uint32_t p_render_subpass) {
    Ref<RenderPipeline> render_pipeline = new RenderPipeline();

    // load the shader program
    Ref<ShaderProgram> shader_program = new ShaderProgram();
    Error err = shader_program->load_shader_program(program_name, vk_device);
    ERR_FAIL_COND_V(err != OK, nullptr);
    ERR_FAIL_COND_V(shader_program->pipeline_type != PIPELINE_TYPE_GRAPHICS, nullptr);

    // vertex input
    Vector<VkPipelineVertexInputStateCreateInfo> vertex_input_state_create_infos;

    // input assembly
    VkPipelineInputAssemblyStateCreateInfo input_assembly_create_info = {};
    input_assembly_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly_create_info.topology = p_render_primitive;
    input_assembly_create_info.primitiveRestartEnable = VK_FALSE;

    // tessellation
    VkPipelineTessellationStateCreateInfo tessellation_create_info = {};
    tessellation_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
    ERR_FAIL_COND_V_MSG(physical_device_properties.limits.maxTessellationPatchSize > 0 &&
                            (p_rasterization_state.patch_control_points < 1 ||
                             p_rasterization_state.patch_control_points >
                                 physical_device_properties.limits.maxTessellationPatchSize),
                        nullptr, "Invalid Vulkan tessellation");
    tessellation_create_info.patchControlPoints = p_rasterization_state.patch_control_points;

    // viewport
    VkPipelineViewportStateCreateInfo viewport_state_create_info = {};
    viewport_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state_create_info.viewportCount = 1;
    viewport_state_create_info.scissorCount = 1;

    // rasterization
    VkPipelineRasterizationStateCreateInfo rasterization_state_create_info = {};
    rasterization_state_create_info.sType =
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization_state_create_info.depthClampEnable = p_rasterization_state.enable_depth_clamp;
    rasterization_state_create_info.rasterizerDiscardEnable =
        p_rasterization_state.discard_primitives;
    rasterization_state_create_info.polygonMode =
        p_rasterization_state.wireframe ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;
    rasterization_state_create_info.cullMode = p_rasterization_state.cull_mode;
    rasterization_state_create_info.frontFace = p_rasterization_state.front_face;
    rasterization_state_create_info.depthBiasEnable = p_rasterization_state.depth_bias_enabled;
    rasterization_state_create_info.depthBiasConstantFactor =
        p_rasterization_state.depth_bias_constant_factor;
    rasterization_state_create_info.depthBiasClamp = p_rasterization_state.depth_bias_clamp;
    rasterization_state_create_info.depthBiasSlopeFactor =
        p_rasterization_state.depth_bias_slope_factor;
    rasterization_state_create_info.lineWidth = p_rasterization_state.line_width;

    // multisample
    VkPipelineMultisampleStateCreateInfo multisample_state_create_info = {};
    multisample_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample_state_create_info.rasterizationSamples = p_multisample_state.sample_count;
    multisample_state_create_info.sampleShadingEnable = p_multisample_state.enable_sample_shading;
    multisample_state_create_info.minSampleShading = p_multisample_state.min_sample_shading;
    if (p_multisample_state.sample_mask.size()) {
        multisample_state_create_info.pSampleMask = p_multisample_state.sample_mask.data();
    } else {
        multisample_state_create_info.pSampleMask = nullptr;
    };

    // depth / stencil
    VkPipelineDepthStencilStateCreateInfo depth_stencil_state_create_info = {};
    depth_stencil_state_create_info.depthTestEnable = p_depth_stencil_state.enable_depth_test;
    depth_stencil_state_create_info.depthWriteEnable = p_depth_stencil_state.enable_depth_write;
    depth_stencil_state_create_info.depthCompareOp = p_depth_stencil_state.depth_compare_operator;
    depth_stencil_state_create_info.depthBoundsTestEnable =
        p_depth_stencil_state.enable_depth_range;
    depth_stencil_state_create_info.stencilTestEnable = p_depth_stencil_state.enable_stencil;

    depth_stencil_state_create_info.front.failOp = p_depth_stencil_state.front_op.fail;
    depth_stencil_state_create_info.front.passOp = p_depth_stencil_state.front_op.pass;
    depth_stencil_state_create_info.front.depthFailOp = p_depth_stencil_state.front_op.depth_fail;
    depth_stencil_state_create_info.front.compareOp = p_depth_stencil_state.front_op.compare;
    depth_stencil_state_create_info.front.compareMask = p_depth_stencil_state.front_op.compare_mask;
    depth_stencil_state_create_info.front.writeMask = p_depth_stencil_state.front_op.write_mask;
    depth_stencil_state_create_info.front.reference = p_depth_stencil_state.front_op.reference;

    depth_stencil_state_create_info.back.failOp = p_depth_stencil_state.back_op.fail;
    depth_stencil_state_create_info.back.passOp = p_depth_stencil_state.back_op.pass;
    depth_stencil_state_create_info.back.depthFailOp = p_depth_stencil_state.back_op.depth_fail;
    depth_stencil_state_create_info.back.compareOp = p_depth_stencil_state.back_op.compare;
    depth_stencil_state_create_info.back.compareMask = p_depth_stencil_state.back_op.compare_mask;
    depth_stencil_state_create_info.back.writeMask = p_depth_stencil_state.back_op.write_mask;
    depth_stencil_state_create_info.back.reference = p_depth_stencil_state.back_op.reference;

    depth_stencil_state_create_info.minDepthBounds = p_depth_stencil_state.depth_range_min;
    depth_stencil_state_create_info.maxDepthBounds = p_depth_stencil_state.depth_range_max;

    // blend state
    VkPipelineColorBlendStateCreateInfo color_blend_state_create_info = {};
    color_blend_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blend_state_create_info.logicOpEnable = p_blend_state.enable_logic_op;
    color_blend_state_create_info.logicOp = p_blend_state.logic_op;

    Vector<VkPipelineColorBlendAttachmentState> vk_attachment_states(p_color_attachments.size());
    for (uint32_t i = 0; i < p_color_attachments.size(); i++) {
        vk_attachment_states[i] = {};
        if (p_color_attachments[i] != VK_ATTACHMENT_UNUSED) {
            vk_attachment_states[i].blendEnable = p_blend_state.attachments[i].enable_blend;

            vk_attachment_states[i].srcColorBlendFactor =
                p_blend_state.attachments[i].src_color_blend_factor;
            vk_attachment_states[i].dstColorBlendFactor =
                p_blend_state.attachments[i].dst_color_blend_factor;
            vk_attachment_states[i].colorBlendOp = p_blend_state.attachments[i].color_blend_op;

            vk_attachment_states[i].srcAlphaBlendFactor =
                p_blend_state.attachments[i].src_alpha_blend_factor;
            vk_attachment_states[i].dstAlphaBlendFactor =
                p_blend_state.attachments[i].dst_alpha_blend_factor;
            vk_attachment_states[i].alphaBlendOp = p_blend_state.attachments[i].alpha_blend_op;

            if (p_blend_state.attachments[i].write_r) {
                vk_attachment_states[i].colorWriteMask |= VK_COLOR_COMPONENT_R_BIT;
            }
            if (p_blend_state.attachments[i].write_g) {
                vk_attachment_states[i].colorWriteMask |= VK_COLOR_COMPONENT_G_BIT;
            }
            if (p_blend_state.attachments[i].write_b) {
                vk_attachment_states[i].colorWriteMask |= VK_COLOR_COMPONENT_B_BIT;
            }
            if (p_blend_state.attachments[i].write_a) {
                vk_attachment_states[i].colorWriteMask |= VK_COLOR_COMPONENT_A_BIT;
            }
        }
    }

    color_blend_state_create_info.attachmentCount = p_color_attachments.size();
    color_blend_state_create_info.pAttachments = vk_attachment_states.data();

    color_blend_state_create_info.blendConstants[0] = p_blend_state.blend_constant.r;
    color_blend_state_create_info.blendConstants[1] = p_blend_state.blend_constant.g;
    color_blend_state_create_info.blendConstants[2] = p_blend_state.blend_constant.b;
    color_blend_state_create_info.blendConstants[3] = p_blend_state.blend_constant.a;

    // dynamic state
    VkPipelineDynamicStateCreateInfo dynamic_state_create_info = {};
    dynamic_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state_create_info.dynamicStateCount = p_dynamic_state.size();
    dynamic_state_create_info.pDynamicStates = p_dynamic_state.data();

    VkGraphicsPipelineCreateInfo pipeline_create_info = {};
    pipeline_create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_create_info.stageCount = 0;  // p_shader->vk_stages_create_info.size();
    ERR_FAIL_COND_V_MSG(pipeline_create_info.stageCount != 0, nullptr,
                        "Can't create Vulkan pipeline without shader module");

    pipeline_create_info.stageCount = shader_program->vk_pipeline_stages.size();
    pipeline_create_info.pStages = shader_program->vk_pipeline_stages.data();
    pipeline_create_info.pVertexInputState = &vertex_input;
    pipeline_create_info.pInputAssemblyState = &input_assembly_create_info;
    pipeline_create_info.pTessellationState = &tessellation_create_info;
    pipeline_create_info.pViewportState = &viewport_state_create_info;
    pipeline_create_info.pRasterizationState = &rasterization_state_create_info;
    pipeline_create_info.pMultisampleState = &multisample_state_create_info;
    pipeline_create_info.pDepthStencilState = &depth_stencil_state_create_info;
    pipeline_create_info.pColorBlendState = &color_blend_state_create_info;
    pipeline_create_info.pDynamicState = &dynamic_state_create_info;
    pipeline_create_info.layout = shader_program->vk_pipeline_layout;
    pipeline_create_info.renderPass = p_render_pass;
    pipeline_create_info.subpass = p_render_subpass;

    VkResult result = vkCreateGraphicsPipelines(vk_device, nullptr, 1, &pipeline_create_info,
                                                nullptr, &render_pipeline->vk_pipeline);
    ERR_FAIL_COND_V_MSG(result, nullptr, "Couldn't create Vulkan graphics pipelines");

    render_pipeline->shader_program = shader_program;

    return render_pipeline;
}

/* swapchain */
Ref<Swapchain> RDD::swapchain_create(Surface &p_surface) {
    Ref<Swapchain> swapchain = new Swapchain();

    VkSurfaceCapabilitiesKHR surface_capabilities = {};
    VkResult err = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, p_surface.vk_surface,
                                                             &surface_capabilities);
    ERR_FAIL_COND_V_MSG(err != VK_SUCCESS, nullptr, "Couldn't get Vulkan surface capabilities");

    if (surface_capabilities.currentExtent.width != 0xFFFFFFFF) {
        surface_capabilities.currentExtent.width =
            std::clamp(p_surface.width, surface_capabilities.minImageExtent.width,
                       surface_capabilities.maxImageExtent.width);
        surface_capabilities.currentExtent.height =
            std::clamp(p_surface.height, surface_capabilities.minImageExtent.height,
                       surface_capabilities.maxImageExtent.height);
    }

    VkExtent2D extent;
    if (surface_capabilities.currentExtent.width == 0xFFFFFFFF) {
        extent.width = std::clamp(p_surface.width, surface_capabilities.minImageExtent.width,
                                  surface_capabilities.maxImageExtent.width);
        extent.height = std::clamp(p_surface.height, surface_capabilities.minImageExtent.height,
                                   surface_capabilities.maxImageExtent.height);
    } else {
        extent = surface_capabilities.currentExtent;
        p_surface.width = extent.width;
        p_surface.height = extent.height;
    }

    if (p_surface.width == 0 || p_surface.height == 0) {
        return nullptr;
    }

    Vector<VkPresentModeKHR> present_modes;
    uint32_t present_modes_count = 0;

    err = vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, p_surface.vk_surface,
                                                    &present_modes_count, nullptr);
    ERR_FAIL_COND_V_MSG(err != VK_SUCCESS, nullptr, "Couldn't get Vulkan surface present modes");

    present_modes.resize(present_modes_count);
    err = vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, p_surface.vk_surface,
                                                    &present_modes_count, present_modes.data());
    ERR_FAIL_COND_V_MSG(err != VK_SUCCESS, nullptr, "Couldn't get Vulkan surface present modes");

    VkPresentModeKHR present_mode = VkPresentModeKHR::VK_PRESENT_MODE_FIFO_KHR;
    String present_mode_name = "Enabled";
    switch (p_surface.vsync_mode) {
        case VSYNC_MAILBOX:
            present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
            present_mode_name = "Mailbox";
            break;
        case VSYNC_ADAPTIVE:
            present_mode = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
            present_mode_name = "Adaptive";
            break;
        case VSYNC_ENABLED:
            present_mode = VK_PRESENT_MODE_FIFO_KHR;
            present_mode_name = "Enabled";
            break;
        case VSYNC_DISABLED:
            present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
            present_mode_name = "Disabled";
            break;
    }

    if (std::find(present_modes.begin(), present_modes.end(), present_mode) ==
        present_modes.end()) {
        p_surface.vsync_mode = VSYNC_ENABLED;
        present_mode = VkPresentModeKHR::VK_PRESENT_MODE_FIFO_KHR;
    }

    VkFormat format = VK_FORMAT_UNDEFINED;
    VkColorSpaceKHR color_space = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

    if (!_determine_swap_chain_format(p_surface, format, color_space)) {
        ERR_FAIL_V_MSG(nullptr, "Vulkan surface did not return any valid formats");
    } else {
        swapchain->format = format;
        swapchain->color_space = color_space;
    };

    uint32_t desired_swapchain_images = surface_capabilities.minImageCount;
    VkSurfaceTransformFlagBitsKHR surface_transform_bits = surface_capabilities.currentTransform;
    VkCompositeAlphaFlagBitsKHR composite_alpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

    VkSwapchainCreateInfoKHR swapchain_create_info = {};
    swapchain_create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchain_create_info.pNext = nullptr;
    swapchain_create_info.flags = 0;
    swapchain_create_info.surface = p_surface.vk_surface;
    swapchain_create_info.minImageCount = desired_swapchain_images;
    swapchain_create_info.imageFormat = swapchain->format;
    swapchain_create_info.imageColorSpace = swapchain->color_space;
    swapchain_create_info.imageExtent = extent;
    swapchain_create_info.imageArrayLayers = 1;
    swapchain_create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchain_create_info.queueFamilyIndexCount = surface_transform_bits;
    swapchain_create_info.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    swapchain_create_info.compositeAlpha = composite_alpha;
    swapchain_create_info.presentMode = present_mode;
    swapchain_create_info.clipped = VK_TRUE;
    swapchain_create_info.oldSwapchain = VK_NULL_HANDLE;

    VkResult result =
        vkCreateSwapchainKHR(vk_device, &swapchain_create_info, nullptr, &swapchain->vk_swapchain);
    ERR_FAIL_COND_V_MSG(result != VK_SUCCESS, nullptr, "Couldn't able to create swapchain");

    uint32_t image_count = 0;
    result = vkGetSwapchainImagesKHR(vk_device, swapchain->vk_swapchain, &image_count, nullptr);
    ERR_FAIL_COND_V_MSG(result != VK_SUCCESS, nullptr, "Couldn't get Vulkan swapchain images");

    swapchain->images.resize(image_count);
    result = vkGetSwapchainImagesKHR(vk_device, swapchain->vk_swapchain, &image_count,
                                     swapchain->images.data());
    ERR_FAIL_COND_V_MSG(result != VK_SUCCESS, nullptr, "Couldn't get Vulkan swapchain images");

    VkImageViewCreateInfo view_create_info = {};
    view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_create_info.format = swapchain->format;
    view_create_info.components.r = VK_COMPONENT_SWIZZLE_R;
    view_create_info.components.g = VK_COMPONENT_SWIZZLE_G;
    view_create_info.components.b = VK_COMPONENT_SWIZZLE_B;
    view_create_info.components.a = VK_COMPONENT_SWIZZLE_A;
    view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view_create_info.subresourceRange.levelCount = 1;
    view_create_info.subresourceRange.layerCount = 1;

    swapchain->image_views.reserve(image_count);

    VkImageView image_view;
    for (uint32_t i = 0; i < image_count; i++) {
        view_create_info.image = swapchain->images[i];
        result = vkCreateImageView(vk_device, &view_create_info, nullptr, &image_view);
        ERR_FAIL_COND_V_MSG(result != VK_SUCCESS, nullptr,
                            "Couldn't create Vulkan image view for swapchain image");

        swapchain->image_views.push_back(image_view);
    }

    swapchain->framebuffers.reserve(image_count);

    VkAttachmentDescription2KHR attachment = {};
    attachment.sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2_KHR;
    attachment.format = format;
    attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference2KHR color_reference = {};
    color_reference.sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2_KHR;
    color_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription2KHR subpass = {};
    subpass.sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2_KHR;
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_reference;

    VkRenderPassCreateInfo2KHR pass_info = {};
    pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2_KHR;
    pass_info.attachmentCount = 1;
    pass_info.pAttachments = &attachment;
    pass_info.subpassCount = 1;
    pass_info.pSubpasses = &subpass;

    VkRenderPass vk_render_pass = VK_NULL_HANDLE;
    _create_render_pass(vk_device, &pass_info, nullptr, &vk_render_pass);
    ERR_FAIL_COND_V_MSG(err != VK_SUCCESS, nullptr,
                        "Couldn't create Vulkan render pass for swapchain");

    swapchain->vk_render_pass = vk_render_pass;

    VkFramebufferCreateInfo fb_create_info = {};
    fb_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fb_create_info.renderPass = vk_render_pass;
    fb_create_info.attachmentCount = 1;
    fb_create_info.width = p_surface.width;
    fb_create_info.height = p_surface.height;
    fb_create_info.layers = 1;

    VkFramebuffer vk_framebuffer;
    for (uint32_t i = 0; i < image_count; i++) {
        fb_create_info.pAttachments = &swapchain->image_views[i];
        err = vkCreateFramebuffer(vk_device, &fb_create_info, nullptr, &vk_framebuffer);
        ERR_FAIL_COND_V_MSG(err != VK_SUCCESS, nullptr,
                            "Couldn't create Vulkan framebuffer for swapchain image");

        Ref<Framebuffer> framebuffer = new Framebuffer();
        framebuffer->vk_framebuffer = vk_framebuffer;
        framebuffer->swap_chain_image = swapchain->images[i];
        framebuffer->swap_chain_image_subresource_range = view_create_info.subresourceRange;
        swapchain->framebuffers.push_back(framebuffer);
    }

    VkSemaphore vk_semaphore = VK_NULL_HANDLE;
    for (uint32_t i = 0; i < image_count; i++) {
        VkSemaphoreCreateInfo create_info = {};
        create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        err = vkCreateSemaphore(vk_device, &create_info, nullptr, &vk_semaphore);
        ERR_FAIL_COND_V_MSG(err != VK_SUCCESS, nullptr,
                            "Couldn't create Vulkan semaphore for swapchain image");

        swapchain->present_semaphores.push_back(vk_semaphore);
    }

    return swapchain;
}

bool RDD::_determine_swap_chain_format(Surface &p_surface, VkFormat &p_format,
                                       VkColorSpaceKHR &p_color_space) {
    uint32_t format_count = 0;
    Vector<VkSurfaceFormatKHR> formats;

    while (true) {
        VkResult err = vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, p_surface.vk_surface,
                                                            &format_count, nullptr);
        ERR_FAIL_COND_V_MSG(err != VK_SUCCESS, false, "Couldn't retrieve Vulkan surface formats");

        formats.resize(format_count);
        err = vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, p_surface.vk_surface,
                                                   &format_count, formats.data());
        ERR_FAIL_COND_V_MSG(err != VK_SUCCESS, false, "Couldn't retrieve Vulkan surface formats");
        if (err == VK_SUCCESS) {
            break;
        }

        ERR_FAIL_COND_V_MSG(err != VK_INCOMPLETE, false,
                            "Couldn't retrieve Vulkan surface formats");
    }
    formats.resize(format_count);

    if (format_count == 1 && formats[0].format == VK_FORMAT_UNDEFINED) {
        p_format = VK_FORMAT_B8G8R8A8_UNORM;
        p_color_space = formats[0].colorSpace;
        return true;
    }

    struct FormatCandidate {
        VkFormat format;
        VkColorSpaceKHR colorspace;
    };

    Vector<FormatCandidate> preferred_formats;
    preferred_formats.push_back({VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR});
    preferred_formats.push_back({VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR});

    bool found = false;

    for (const FormatCandidate &candidate : preferred_formats) {
        for (uint32_t i = 0; i < format_count; i++) {
            if (formats[i].format == candidate.format &&
                formats[i].colorSpace == candidate.colorspace) {
                p_format = formats[i].format;
                p_color_space = formats[i].colorSpace;
                found = true;
                break;
            }

            if (found) {
                break;
            }
        }
    }

    return found;
}

void RDD::swapchain_acquire_next_image(Ref<Swapchain> p_swapchain, Ref<Semaphore> p_semaphore,
                                       Ref<Fence> p_fence) {
    vkAcquireNextImageKHR(vk_device, p_swapchain->vk_swapchain, UINT64_MAX,
                          p_semaphore->vk_semaphore, VK_NULL_HANDLE, &p_swapchain->image_index);
}

Error RDD::swapchain_resize(Ref<Swapchain> p_old_swapchain) {
    if (enabled_device_extension_names.find(VK_KHR_SHARED_PRESENTABLE_IMAGE_EXTENSION_NAME) !=
            enabled_device_extension_names.end() &&
        p_old_swapchain) {
        // check the swapchain status
        // VkResult result = vkGetSwapchainStatusKHR(vk_device, p_old_swapchain->vk_swapchain);
    }

    return OK;
}

Ref<Framebuffer> RDD::swapchain_acquire_framebuffer() { return nullptr; }

void RDD::swapchain_free(Ref<Swapchain> swapchain) {
    vkDestroySwapchainKHR(vk_device, swapchain->vk_swapchain, nullptr);
}

static void _convert_subpass_attachments(const VkAttachmentReference2 *p_attachment_references_2,
                                         uint32_t p_attachment_references_count,
                                         Vector<VkAttachmentReference> &r_attachment_references) {
    r_attachment_references.resize(p_attachment_references_count);
    for (uint32_t i = 0; i < p_attachment_references_count; i++) {
        // Ignore sType, pNext and aspectMask (which is currently unused).
        r_attachment_references[i].attachment = p_attachment_references_2[i].attachment;
        r_attachment_references[i].layout = p_attachment_references_2[i].layout;
    }
}

VkResult RDD::_create_render_pass(VkDevice p_device, const VkRenderPassCreateInfo2 *p_create_info,
                                  const VkAllocationCallbacks *p_allocator,
                                  VkRenderPass *p_render_pass) {
    if (vkCreateRenderPass2KHR) {
        return vkCreateRenderPass2KHR(p_device, p_create_info, p_allocator, p_render_pass);
    } else {
        Vector<VkAttachmentDescription> attachments;
        attachments.resize(p_create_info->attachmentCount);
        for (uint32_t i = 0; i < p_create_info->attachmentCount; i++) {
            const VkAttachmentDescription2 &src = p_create_info->pAttachments[i];
            VkAttachmentDescription &dst = attachments[i];
            dst.flags = src.flags;
            dst.format = src.format;
            dst.samples = src.samples;
            dst.loadOp = src.loadOp;
            dst.storeOp = src.storeOp;
            dst.stencilLoadOp = src.stencilLoadOp;
            dst.stencilStoreOp = src.stencilStoreOp;
            dst.initialLayout = src.initialLayout;
            dst.finalLayout = src.finalLayout;
        }

        const uint32_t attachment_vectors_per_subpass = 4;
        Vector<Vector<VkAttachmentReference>> subpasses_attachments;
        Vector<VkSubpassDescription> subpasses;
        subpasses_attachments.resize(p_create_info->subpassCount * attachment_vectors_per_subpass);
        subpasses.resize(p_create_info->subpassCount);
        for (uint32_t i = 0; i < p_create_info->subpassCount; i++) {
            const uint32_t vector_base_index = i * attachment_vectors_per_subpass;
            const uint32_t input_attachments_index = vector_base_index + 0;
            const uint32_t color_attachments_index = vector_base_index + 1;
            const uint32_t resolve_attachments_index = vector_base_index + 2;
            const uint32_t depth_attachment_index = vector_base_index + 3;
            _convert_subpass_attachments(p_create_info->pSubpasses[i].pInputAttachments,
                                         p_create_info->pSubpasses[i].inputAttachmentCount,
                                         subpasses_attachments[input_attachments_index]);
            _convert_subpass_attachments(p_create_info->pSubpasses[i].pColorAttachments,
                                         p_create_info->pSubpasses[i].colorAttachmentCount,
                                         subpasses_attachments[color_attachments_index]);
            _convert_subpass_attachments(
                p_create_info->pSubpasses[i].pResolveAttachments,
                (p_create_info->pSubpasses[i].pResolveAttachments != nullptr)
                    ? p_create_info->pSubpasses[i].colorAttachmentCount
                    : 0,
                subpasses_attachments[resolve_attachments_index]);
            _convert_subpass_attachments(
                p_create_info->pSubpasses[i].pDepthStencilAttachment,
                (p_create_info->pSubpasses[i].pDepthStencilAttachment != nullptr) ? 1 : 0,
                subpasses_attachments[depth_attachment_index]);

            // Ignores sType and pNext from the subpass.
            const VkSubpassDescription2 &src_subpass = p_create_info->pSubpasses[i];
            VkSubpassDescription &dst_subpass = subpasses[i];
            dst_subpass.flags = src_subpass.flags;
            dst_subpass.pipelineBindPoint = src_subpass.pipelineBindPoint;
            dst_subpass.inputAttachmentCount = src_subpass.inputAttachmentCount;
            dst_subpass.pInputAttachments = subpasses_attachments[input_attachments_index].data();
            dst_subpass.colorAttachmentCount = src_subpass.colorAttachmentCount;
            dst_subpass.pColorAttachments = subpasses_attachments[color_attachments_index].data();
            dst_subpass.pResolveAttachments =
                subpasses_attachments[resolve_attachments_index].data();
            dst_subpass.pDepthStencilAttachment =
                subpasses_attachments[depth_attachment_index].data();
            dst_subpass.preserveAttachmentCount = src_subpass.preserveAttachmentCount;
            dst_subpass.pPreserveAttachments = src_subpass.pPreserveAttachments;

            Vector<VkSubpassDependency> dependencies;
            dependencies.resize(p_create_info->dependencyCount);

            for (uint32_t i = 0; i < p_create_info->dependencyCount; i++) {
                // Ignores sType and pNext from the dependency, and viewMask which is currently
                // unused.
                const VkSubpassDependency2 &src_dependency = p_create_info->pDependencies[i];
                VkSubpassDependency &dst_dependency = dependencies[i];
                dst_dependency.srcSubpass = src_dependency.srcSubpass;
                dst_dependency.dstSubpass = src_dependency.dstSubpass;
                dst_dependency.srcStageMask = src_dependency.srcStageMask;
                dst_dependency.dstStageMask = src_dependency.dstStageMask;
                dst_dependency.srcAccessMask = src_dependency.srcAccessMask;
                dst_dependency.dstAccessMask = src_dependency.dstAccessMask;
                dst_dependency.dependencyFlags = src_dependency.dependencyFlags;
            }
        }

        Vector<VkSubpassDependency> dependencies;
        dependencies.resize(p_create_info->dependencyCount);
        for (uint32_t i = 0; i < p_create_info->dependencyCount; i++) {
            const VkSubpassDependency2 &src_dependency = p_create_info->pDependencies[i];
            VkSubpassDependency &dst_dependency = dependencies[i];
            dst_dependency.srcSubpass = src_dependency.srcSubpass;
            dst_dependency.dstSubpass = src_dependency.dstSubpass;
            dst_dependency.srcStageMask = src_dependency.srcStageMask;
            dst_dependency.dstStageMask = src_dependency.dstStageMask;
            dst_dependency.srcAccessMask = src_dependency.srcAccessMask;
            dst_dependency.dstAccessMask = src_dependency.dstAccessMask;
            dst_dependency.dependencyFlags = src_dependency.dependencyFlags;
        }

        VkRenderPassCreateInfo create_info = {};
        create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        create_info.pNext = p_create_info->pNext;
        create_info.flags = p_create_info->flags;
        create_info.attachmentCount = attachments.size();
        create_info.pAttachments = attachments.data();
        create_info.subpassCount = subpasses.size();
        create_info.pSubpasses = subpasses.data();
        create_info.dependencyCount = dependencies.size();
        create_info.pDependencies = dependencies.data();
        return vkCreateRenderPass(vk_device, &create_info, p_allocator, p_render_pass);
    }
}

/* uniform sets */
VkDescriptorPool RDD::_descriptor_set_pool_create(const VkDescriptorType &p_type) {
    for (const auto &[key, value] : descriptor_set_pools) {
        if (key == p_type) return value;
    }

    VkDescriptorPoolSize pool_sizes = {};
    pool_sizes.type = p_type;
    pool_sizes.descriptorCount = max_descriptor_sets_per_pool;

    VkDescriptorPoolCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    create_info.flags = 0;
    create_info.maxSets = max_descriptor_sets_per_pool;
    create_info.poolSizeCount = 1;
    create_info.pPoolSizes = &pool_sizes;

    VkDescriptorPool vk_descriptor_pool = VK_NULL_HANDLE;

    VkResult res = vkCreateDescriptorPool(vk_device, &create_info, nullptr, &vk_descriptor_pool);
    ERR_FAIL_COND_V_MSG(res, VK_NULL_HANDLE, "Couldn't create Vulkan descriptor pool");

    descriptor_set_pools.insert(std::pair<uint32_t, VkDescriptorPool>(p_type, vk_descriptor_pool));

    return vk_descriptor_pool;
}

void RDD::_descriptor_set_pool_unreference(VkDescriptorPool p_vk_descriptor_pool) {}

Ref<UniformSet> RDD::uniform_set_create(Vector<BoundUniform> p_uniforms,
                                        Ref<ShaderProgram> p_shader, uint32_t p_set_index) {
    Ref<UniformSet> uniform_set = new UniformSet();

    VkWriteDescriptorSet *vk_writes =
        (VkWriteDescriptorSet *)alloca(sizeof(VkWriteDescriptorSet) * p_uniforms.size());
    uint32_t writes_amount = 0;
    for (uint32_t i = 0; i < p_uniforms.size(); ++i) {
        const BoundUniform &uniform = p_uniforms[i];

        vk_writes[writes_amount] = {};
        vk_writes[writes_amount].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;

        bool add_write = true;
        uint32_t num_descriptors = 1;

        switch (uniform.type) {
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER: {
                Ref<Buffer> buffer = uniform.buffers[0];

                VkDescriptorBufferInfo *vk_buf_info =
                    (VkDescriptorBufferInfo *)alloca(sizeof(VkDescriptorImageInfo));
                *vk_buf_info = {};
                vk_buf_info->buffer = buffer->vk_buffer;
                vk_buf_info->range = buffer->size;

                vk_writes[writes_amount].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                vk_writes[writes_amount].pBufferInfo = vk_buf_info;
            } break;

            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER: {
                Ref<Buffer> buffer = uniform.buffers[0];

                VkDescriptorBufferInfo *vk_buf_info =
                    (VkDescriptorBufferInfo *)alloca(sizeof(VkDescriptorImageInfo));
                *vk_buf_info = {};
                vk_buf_info->buffer = buffer->vk_buffer;
                vk_buf_info->range = buffer->size;

                vk_writes[writes_amount].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                vk_writes[writes_amount].pBufferInfo = vk_buf_info;

            } break;
            default:
                ERR_FAIL_V_MSG(nullptr,
                               "Descriptor Set other than VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER and "
                               "UNIFORM_TYPE_STORAGE_BUFFER not supported");
        }

        if (add_write) {
            vk_writes[writes_amount].dstBinding = uniform.binding;
            vk_writes[writes_amount].descriptorCount = num_descriptors;
            writes_amount++;
        }
    }

    VkDescriptorSetAllocateInfo descriptor_set_allocate_info = {};
    descriptor_set_allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptor_set_allocate_info.descriptorSetCount = 1;
    descriptor_set_allocate_info.pSetLayouts = &p_shader->descriptor_set_layouts[p_set_index];
    descriptor_set_allocate_info.descriptorPool =
        _descriptor_set_pool_create(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

    VkDescriptorSet vk_descriptor_set = VK_NULL_HANDLE;
    VkResult res =
        vkAllocateDescriptorSets(vk_device, &descriptor_set_allocate_info, &vk_descriptor_set);
    ERR_FAIL_COND_V_MSG(res, nullptr, "Couldn't allocate Vulkan descriptor sets");

    for (uint32_t i = 0; i < writes_amount; i++) {
        vk_writes[i].dstSet = vk_descriptor_set;
    }
    vkUpdateDescriptorSets(vk_device, writes_amount, vk_writes, 0, nullptr);

    uniform_set->vk_descriptor_set = vk_descriptor_set;

    return uniform_set;
}

void RDD::uniform_set_free(Ref<UniformSet> p_uniform_set) {}

void RDD::command_bind_render_pipeline(Ref<CommandBuffer> p_cmd_buffer,
                                       Ref<RenderPipeline> p_render_pipeline) {
    vkCmdBindPipeline(p_cmd_buffer->vk_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      p_render_pipeline->vk_pipeline);
}

void RDD::command_bind_render_uniform_sets(Ref<CommandBuffer> p_cmd_buffer,
                                           Vector<Ref<UniformSet>> p_uniforms,
                                           Ref<ShaderProgram> p_shader, uint32_t p_first_set_index,
                                           uint32_t p_set_count) {
    Vector<VkDescriptorSet> sets;
    sets.clear();
    sets.resize(p_set_count);

    for (uint32_t i = 0; i < p_set_count; ++i) {
        sets[i] = p_uniforms[i]->vk_descriptor_set;
    }

    vkCmdBindDescriptorSets(p_cmd_buffer->vk_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            p_shader->vk_pipeline_layout, p_first_set_index, p_set_count, &sets[0],
                            0, nullptr);
}

// drawing
void RDD::command_render_draw(Ref<CommandBuffer> p_cmd_buffer, uint32_t p_vertex_count,
                              uint32_t p_instance_count, uint32_t p_base_vertex,
                              uint32_t p_first_instance) {
    vkCmdDraw(p_cmd_buffer->vk_command_buffer, p_vertex_count, p_instance_count, p_base_vertex,
              p_first_instance);
}

void RDD::command_render_draw_indexed(Ref<CommandBuffer> p_cmd_buffer, uint32_t p_index_count,
                                      uint32_t p_instance_count, uint32_t p_first_vertex,
                                      int32_t p_vertex_offset, uint32_t p_first_instance) {
    vkCmdDrawIndexed(p_cmd_buffer->vk_command_buffer, p_index_count, p_instance_count,
                     p_first_vertex, p_vertex_offset, p_first_instance);
}

// buffer binding
void RDD::command_render_bind_vertex_buffers(Ref<CommandBuffer> p_cmd_buffer,
                                             uint32_t p_binding_count, Ref<Buffer> p_buffer,
                                             uint64_t p_offset) {
    vkCmdBindVertexBuffers(p_cmd_buffer->vk_command_buffer, 0, p_binding_count,
                           &p_buffer->vk_buffer, &p_offset);
}

void RDD::command_render_bind_index_buffer(Ref<CommandBuffer> p_cmd_buffer, Ref<Buffer> p_buffer,
                                           VkIndexType p_index_type, uint64_t p_offset) {
    vkCmdBindIndexBuffer(p_cmd_buffer->vk_command_buffer, p_buffer->vk_buffer, p_offset,
                         p_index_type);
}

// dynamic state
void RDD::command_render_set_blend_constants(Ref<CommandBuffer> p_cmd_buffer,
                                             float *p_blend_constants) {
    vkCmdSetBlendConstants(p_cmd_buffer->vk_command_buffer, p_blend_constants);
}

void RDD::command_render_set_line_width(Ref<CommandBuffer> p_cmd_buffer, float p_width) {
    vkCmdSetLineWidth(p_cmd_buffer->vk_command_buffer, p_width);
}

/* compute */
Ref<ComputePipeline> RDD::compute_pipeline_create(const char *p_program_name) {
    Ref<ComputePipeline> pipeline = new ComputePipeline();

    ShaderProgram p_program = ShaderProgram();
    p_program.load_shader_program(p_program_name, vk_device);
    ERR_FAIL_COND_V_MSG(
        !p_program.has_compute_shader || p_program.pipeline_type != PIPELINE_TYPE_COMPUTE, nullptr,
        "Failed to create shader program for compute pipeline");

    VkComputePipelineCreateInfo pipeline_create_info = {};
    pipeline_create_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipeline_create_info.stage = p_program.vk_pipeline_stages[0];
    pipeline_create_info.layout = p_program.vk_pipeline_layout;

    VkResult err = vkCreateComputePipelines(vk_device, VK_NULL_HANDLE, 1, &pipeline_create_info,
                                            nullptr, &pipeline->vk_pipeline);
    ERR_FAIL_COND_V_MSG(err, nullptr, "Couldn't create Vulkan compute pipelines");

    return pipeline;
}

void RDD::command_bind_compute_pipeline(Ref<CommandBuffer> p_cmd_buffer,
                                        Ref<ComputePipeline> p_pipeline) {
    vkCmdBindPipeline(p_cmd_buffer->vk_command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                      p_pipeline->vk_pipeline);
}

void RDD::command_bind_compute_uniform_sets(Ref<CommandBuffer> p_cmd_buffer,
                                            Vector<UniformSet> p_uniform_sets) {}

void RDD::command_compute_dispatch(Ref<CommandBuffer> p_cmd_buffer, uint32_t p_x_groups,
                                   uint32_t p_y_groups, uint32_t p_z_groups) {
    vkCmdDispatch(p_cmd_buffer->vk_command_buffer, p_x_groups, p_y_groups, p_z_groups);
}

RDD::RenderingDeviceDriver(RenderingDriverContext *p_context_driver)
    : context_driver(p_context_driver) {}

RDD::~RenderingDeviceDriver() {}
