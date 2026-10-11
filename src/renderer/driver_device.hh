#pragma once

#include <mutex>

#include "core/containers.hh"
#include "core/error/error_list.hh"
#include "display.hh"
#include "driver_context.hh"
#include "types.hh"

class RenderingDeviceDriver {
    struct Queue {
        VkQueue queue = VK_NULL_HANDLE;
        uint32_t virtual_count = 0;
        // std::mutex submit_mutex;
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
    bool vulkan_memory_model_support = false;
    bool vulkan_memory_model_device_scope_support = false;
    AccelerationStructureCapabilities acceleration_structure_capabilities;
    bool ray_query_support = false;
    RaytracingCapabilities raytracing_capabilities;
    bool pipeline_cache_control_support = false;
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
    VkResult _create_render_pass(VkDevice p_device, const VkRenderPassCreateInfo2 *p_create_info,
                                 const VkAllocationCallbacks *p_allocator,
                                 VkRenderPass *p_render_pass);

public:
    Error initialize(uint32_t p_device_index, uint32_t p_frame_count);

private:
    /* Memory */
    VmaAllocator allocator = nullptr;
    HashMap<uint32_t, VmaPool> small_allocs_pools;

    VmaPool _find_or_create_small_allocs_pool(uint32_t p_mem_type_index);

public:
    enum MemoryAllocationType {
        MEMORY_ALLOCATION_TYPE_CPU,
        MEMORY_ALLOCATION_TYPE_GPU,
        MEMORY_ALLOCATION_TYPE_GPU_MAPPABLE,
    };

    /* buffers */
    Ref<Buffer> buffer_create(uint64_t p_size, VkBufferUsageFlags p_usage,
                              MemoryAllocationType p_allocation_type);
    bool buffer_set_texel_format(Ref<Buffer> p_buffer, VkFormat p_format);
    void buffer_free(Ref<Buffer> p_buffer, VkFormat p_format);
    uint64_t buffer_get_allocation_size(Ref<Buffer> p_buffer);
    uint8_t *buffer_map(Ref<Buffer> p_buffer);
    void buffer_unmap(Ref<Buffer> p_buffer);
    uint64_t buffer_get_device_address(Ref<Buffer> p_buffer);

    /* texture */
    VkSampleCountFlagBits _ensure_supported_sample_count(
        VkSampleCountFlagBits p_requested_sample_count);

public:
    Ref<Texture> texture_create(const TextureFormat &p_format, const TextureView &p_view);
    void texture_free(Ref<Texture> p_texture);

    /* sampler */
public:
    Ref<Sampler> sampler_create(const SamplerState &p_state);
    void sampler_free(Ref<Sampler> p_sampler);
    bool sampler_is_format_supported_for_filter(VkFormat p_format, VkFilter p_filter);

public:
    /* render pass */
    Ref<RenderPass> render_pass_create(Vector<Attachment> &p_attachments,
                                       Vector<Subpass> &p_subpasses,
                                       Vector<SubpassDependency> &p_subpass_dependencies);
    void render_pass_free(Ref<RenderPass> p_render_pass);

    // commands
    void command_begin_render_pass(Ref<CommandBuffer> p_cmd_buffer, Ref<RenderPass> p_render_pass,
                                   Ref<Framebuffer> p_framebuffer, VkCommandBufferLevel p_cmd_level,
                                   const VkRect2D &p_rect, Vector<VkClearValue> p_clear_values);
    void command_end_render_pass(Ref<CommandBuffer> p_cmd_buffer);
    void command_next_render_subpass(Ref<CommandBuffer> p_cmd_buffer,
                                     VkCommandBufferLevel p_cmd_level);
    void command_render_set_viewport(Ref<CommandBuffer> p_cmd_buffer, Vector<VkRect2D> p_viewports);
    void command_render_set_scissor(Ref<CommandBuffer> p_cmd_buffer, Vector<VkRect2D> p_scissors);
    void command_render_clear_attachments(Ref<CommandBuffer> p_cmd_buffer,
                                          Vector<VkClearAttachment> p_attachment_clears,
                                          Vector<VkRect2D> p_rects);

public:
    /* swapchain */
    Ref<Swapchain> swapchain_create(Surface &p_surface);
    Error swapchain_resize(Ref<Swapchain> p_old_swapchain = nullptr);
    void swapchain_acquire_next_image(Ref<Swapchain> p_swapchain, Ref<Semaphore> p_semaphore,
                                      Ref<Fence> p_fence);
    Ref<Framebuffer> swapchain_acquire_framebuffer();
    bool _determine_swap_chain_format(Surface &p_surface, VkFormat &p_format,
                                      VkColorSpaceKHR &p_color_space);
    void swapchain_free(Ref<Swapchain> p_swapchain);

public:
    /* framebuffer */
    Ref<Framebuffer> framebuffer_create(Ref<RenderPass> p_render_pass,
                                        Vector<Ref<Texture>> p_attachments, uint32_t p_width,
                                        uint32_t p_height);
    void framebuffer_free(Ref<Framebuffer> p_framebuffer);

    /* fence */
    Ref<Fence> fence_create();
    Error fence_wait(Ref<Fence> p_fence);
    void fence_free(Ref<Fence> p_fence);

    /* semaphore */
    Ref<Semaphore> semaphore_create();
    void semaphore_free(Ref<Semaphore> p_semaphore);

private:
public:
    Ref<CommandQueue> command_queue_create(int p_cmd_queue_family);
    Error command_queue_execute_and_present(Ref<CommandQueue> p_cmd_queue, Ref<Fence> p_fence,
                                            Ref<Swapchain> p_swapchain,
                                            Vector<Ref<CommandBuffer>> p_cmd_buffers);
    void command_queue_free(Ref<CommandQueue> p_cmd_queue);

    //  /* queues */
    // private:
    //  struct CommandQueue : RefTarget<CommandQueue> {
    //      Vector<VkSemaphore> image_semaphores;
    //      Vector<Swapchain> image_semaphores_swapchains;
    //      Vector<uint32_t> pending_semaphores_for_execute;
    //      Vector<uint32_t> pending_semaphores_for_fences;
    //      Vector<uint32_t> free_image_semaphores;
    //      Vector<std::pair<Ref<Fence>, uint32_t>> imageimage_semaphores_for_fences;
    //      uint32_t queue_family = 0;
    //      uint32_t queue_index = 0;
    //  };
    //
    // public:
    //  Ref<CommandQueue> command_queue_create(uint32_t p_cmd_queue_family,
    //                                         bool p_identify_as_main_queue = false);
    //  Error command_queue_execute_and_present(Ref<CommandQueue>,
    //                                          Vector<Ref<Semaphore>> p_wait_semaphores,
    //                                          Vector<Ref<CommandBuffer>> p_cmd_buffers,
    //                                          Vector<Ref<Semaphore>> p_cmd_semaphores);
    //  void command_queue_free(Ref<CommandQueue> p_cmd_queue);
    //
    // private:
    //  /* uniform sets */
    //  using DescriptorSetPools = HashMap<uint32_t, HashMap<VkDescriptorPool, uint32_t>>;
    //  DescriptorSetPools descriptor_set_pools;
    //  uint32_t max_descriptor_sets_per_pool = 0;
    //
    //  VkDescriptorPool _descriptor_set_pool_create(const uint32_t &p_key);
    //  void _descriptor_set_pool_unreference(VkDescriptorPool p_vk_descriptor_pool);
    //
    // public:
    //  struct BoundUniform {
    //      VkDescriptorType type = VK_DESCRIPTOR_TYPE_MAX_ENUM;
    //      uint32_t binding = 0xffffffff;
    //      bool immutable_sampler = false;
    //      Vector<uint32_t> ids;
    //
    //      _FORCE_INLINE_ bool is_dynamic() const {
    //          return type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC ||
    //                 type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    //      }
    //  };
    //
    //  struct UniformSet : Ref<UniformSet> {
    //      VkDescriptorSet vk_descriptor_set = VK_NULL_HANDLE;
    //      VkDescriptorPool vk_descriptor_pool = VK_NULL_HANDLE;
    //      Vector<Ref<Buffer>> dynamic_buffers;
    //  };
    //
    //  Ref<UniformSet> uniform_set_create(Vector<BoundUniform> p_uniforms);
    //  void uniform_set_free(Ref<UniformSet> p_uniform_set);

    /* commands */
    // command pool
    Ref<CommandPool> command_pool_create(uint32_t queue_family_index,
                                         VkCommandBufferLevel cmd_buffer_level);
    bool command_pool_reset(Ref<CommandPool> p_cmd_pool);
    void command_pool_free(Ref<CommandPool> p_cmd_pool);

    // command buffer
    Ref<CommandBuffer> command_buffer_create(Ref<CommandPool> p_cmd_pool);
    bool command_buffer_begin(Ref<CommandBuffer> p_cmd_buffer);
    bool command_buffer_begin_secondary(Ref<CommandBuffer> p_cmd_buffer);
    void command_buffer_end(Ref<CommandBuffer> p_cmd_buffer);
    void command_buffer_execute_secondary(Ref<CommandBuffer> p_cmd_buffer,
                                          Vector<Ref<CommandBuffer>> p_secondary_cmd_buffers);

public:
    /* pipeline */
    Ref<RenderPipeline> render_pipeline_create(
        String program_name, VkPrimitiveTopology p_render_primitive,
        PipelineRasterizationState p_rasterization_state,
        PipelineMultisampleState p_multisample_state,
        PipelineDepthStencilState p_depth_stencil_state, PipelineColorBlendState p_blend_state,
        Vector<int32_t> p_color_attachments, Vector<VkDynamicState> p_dynamic_state,
        Ref<RenderPass> p_render_pass, uint32_t p_render_subpass);

public:
    RenderingDeviceDriver(RenderingDriverContext *p_context_driver);
    ~RenderingDeviceDriver();
};

using RDD = RenderingDeviceDriver;
