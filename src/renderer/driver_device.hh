#pragma once

#include "core/containers.hh"
#include "core/error/error_list.hh"
#include "driver_context.hh"
#include "types.hh"

class RenderingDeviceDriver {
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

    /* texture */
   public:
    Ref<Texture> texture_create(const TextureFormat &p_format, const TextureView &p_view);
    void texture_free(Ref<Texture> p_texture);

    /* sampler */
   public:
    Ref<Sampler> sampler_create(const SamplerState &p_state);
    void sampler_free(Ref<Sampler> p_sampler);

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
    RenderingDeviceDriver(RenderingDriverContext *p_context_driver);
    ~RenderingDeviceDriver();
};

using RDD = RenderingDeviceDriver;
