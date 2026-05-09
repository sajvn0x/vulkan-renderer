#pragma once

#include "core/containers.hh"
#include "core/error/error_list.hh"
#include "driver_context.hh"
#include "types.hh"

class RenderingDeviceDriver {
    struct CommandQueue;
    struct SwapChain;
    struct CommandBufferInfo;
    struct RenderPassInfo;
    struct FrameBuffer;

    struct Queue {
        VkQueue queue = VK_NULL_HANDLE;
        uint32_t virtual_count = 0;
    };

    VkDevice vk_device = VK_NULL_HANDLE;
    RenderingDriverContext *context_driver = nullptr;
    Device context_device;
    uint32_t frame_count = 1;
    VkPhysicalDevice physical_device = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties physical_device_properties = {};
    VkPhysicalDeviceFeatures physical_device_features = {};
    VkPhysicalDeviceFeatures requested_device_features = {};
    HashMap<String, bool> requested_device_extensions;
    HashSet<String> enabled_device_extension_names;
    Vector<Vector<Queue>> queue_families;
    Vector<VkQueueFamilyProperties> queue_family_properties;

    /* capabilities */
    SubgroupCapabilities subgroup_capabilities;
    MultiviewCapabilities multiview_capabilities;
    FragmentShadingRateCapabilities fsr_capabilities;
    FragmentDensityMapCapabilities fdm_capabilities;
    ShaderCapabilities shader_capabilities;
    StorageBufferCapabilities storage_buffer_capabilities;

    bool buffer_device_address_support = false;
    bool pipeline_cache_control_support = false;
    bool vulkan_memory_model_support = false;
    bool vulkan_memory_model_device_scope_support = false;
    bool device_fault_support = false;
    bool framebuffer_depth_resolve = false;

   private:
    void _register_requested_device_extension(const String &p_extension_name, bool p_required);
    Error _initialize_device_extensions();
    Error _check_device_features();
    Error _check_device_capabilities();

    Error _add_queue_create_info(Vector<VkDeviceQueueCreateInfo> &r_queue_create_info);
    Error _initialize_device(const Vector<VkDeviceQueueCreateInfo> &p_queue_create_info);
    Error _initialize_allocator();

   public:
    Error initialize(uint32_t p_device_index, uint32_t p_frame_count);

   private:
    /* Memory */
    VmaAllocator allocator = nullptr;

   public:
    /* buffers */
    Ref<Buffer> buffer_create(uint64_t p_size, VkBufferUsageFlags p_usage,
                              VmaMemoryUsage p_vma_usage);
    void buffer_free(Ref<Buffer> p_buffer, VkFormat p_format);
    uint64_t buffer_get_allocation_size(Ref<Buffer> p_buffer);
    uint8_t *buffer_map(Ref<Buffer> p_buffer);
    void buffer_unmap(Ref<Buffer> p_buffer);

    /* sampler */
   public:
    Ref<Sampler> sampler_create(const SamplerState &p_state);
    void sampler_free(Ref<Sampler> p_sampler);

   public:
    RenderingDeviceDriver(RenderingDriverContext *p_context_driver);
    ~RenderingDeviceDriver();
};

using RDD = RenderingDeviceDriver;
