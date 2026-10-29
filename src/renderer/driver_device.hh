#pragma once

#include "core/containers.hh"
#include "core/error/error_list.hh"
#include "display.hh"
#include "driver_context.hh"
#include "renderer/shader_container.hh"
#include "types.hh"

#define ALLOCA(m_size) ((m_size != 0) ? alloca(m_size) : nullptr)
#define ALLOCA_ARRAY(m_type, m_count) ((m_type *)ALLOCA(sizeof(m_type) * (m_count)))
#define ALLOCA_SINGLE(m_type) ALLOCA_ARRAY(m_type, 1)

#define MAX_FRAMES_IN_FLIGHT 2

class RenderingDeviceDriver {
public:
    VkDevice get_device() { return vk_device; }

private:
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
    VkPhysicalDeviceFeatures2 physical_device_features2 = {};
    VkPhysicalDeviceVulkan11Features physical_device_features11 = {};
    VkPhysicalDeviceVulkan12Features physical_device_features12 = {};
    VkPhysicalDeviceVulkan13Features physical_device_features13 = {};
    VkPhysicalDeviceVulkan14Features physical_device_features14 = {};
    VkPhysicalDeviceFeatures requested_device_features = {};
    VkPhysicalDeviceFeatures2 requested_device_features2 = {};
    VkPhysicalDeviceVulkan13Features requested_device_features13 = {};

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

    /* Synchronization */
public:
    // fence
    FenceID fence_create();
    Error fence_wait(FenceID p_fence);
    void fence_free(FenceID p_fence);

    // semaphore
    SemaphoreID semaphore_create();
    void semaphore_free(SemaphoreID p_semaphore);

    // pipeline barrier
    void command_pipeline_barrier(CommandBufferID p_cmd_buffer, VkMemoryBarrier2 &p_memory_barrier,
                                  Vector<VkBufferMemoryBarrier2> &p_buffer_barriers,
                                  Vector<VkImageMemoryBarrier2> &p_image_barriers);

    /* memory */
private:
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
private:
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

    /* framebuffer */
public:
    Ref<Framebuffer> framebuffer_create(Ref<RenderPass> p_render_pass,
                                        Vector<Ref<Texture>> p_attachments, uint32_t p_width,
                                        uint32_t p_height);
    void framebuffer_free(Ref<Framebuffer> p_framebuffer);

    /* render pass */
public:
    Ref<RenderPass> render_pass_create(Vector<Attachment> &p_attachments,
                                       Vector<Subpass> &p_subpasses,
                                       Vector<SubpassDependency> &p_subpass_dependencies);
    void render_pass_free(Ref<RenderPass> p_render_pass);

    // dynamic rendering
    void command_render_begin_rendering(Ref<CommandBuffer> p_cmd_buffer, VkRect2D p_render_area);
    void command_render_end_rendering(Ref<CommandBuffer> p_cmd_buffer);

public:
    /* swapchain */
    Ref<Swapchain> swapchain_create(Surface &p_surface);
    Error swapchain_resize(Ref<Swapchain> p_old_swapchain = nullptr);
    void swapchain_acquire_next_image(Ref<Swapchain> p_swapchain, SemaphoreID p_semaphore);
    Ref<Framebuffer> swapchain_acquire_framebuffer();
    bool _determine_swap_chain_format(Surface &p_surface, VkFormat &p_format,
                                      VkColorSpaceKHR &p_color_space);
    void swapchain_free(Ref<Swapchain> p_swapchain);

    /* command queue */
private:
    uint32_t command_queue_family_get();

public:
    Ref<CommandQueue> command_queue_create(int p_cmd_queue_family, uint32_t p_image_count);
    Error command_queue_execute_and_present(Ref<CommandQueue> p_cmd_queue, FenceID p_fence,
                                            Ref<Swapchain> p_swapchain,
                                            Ref<CommandBuffer> p_cmd_buffer,
                                            uint32_t p_frame_index);
    void command_queue_free(Ref<CommandQueue> p_cmd_queue);

    /* uniform sets */
private:
    static const uint32_t MAX_UNIFORM_POOL_ELEMENT = 65535;

    using DescriptorSetPools = HashMap<uint32_t, VkDescriptorPool>;
    DescriptorSetPools descriptor_set_pools;
    uint32_t max_descriptor_sets_per_pool = 100;

    VkDescriptorPool _descriptor_set_pool_create(const VkDescriptorType &p_key);
    void _descriptor_set_pool_unreference(VkDescriptorPool p_vk_descriptor_pool);

public:
    Ref<UniformSet> uniform_set_create(Vector<BoundUniform> p_uniforms, Ref<ShaderProgram> p_shader,
                                       uint32_t p_set_index);
    void uniform_set_free(Ref<UniformSet> p_uniform_set);

    /* commands */
    Ref<CommandPool> command_pool_create(uint32_t queue_family_index,
                                         VkCommandBufferLevel cmd_buffer_level);
    bool command_pool_reset(Ref<CommandPool> p_cmd_pool);
    void command_pool_free(Ref<CommandPool> p_cmd_pool);

    Ref<CommandBuffer> command_buffer_create(Ref<CommandPool> p_cmd_pool);
    bool command_buffer_begin(Ref<CommandBuffer> p_cmd_buffer);
    bool command_buffer_begin_secondary(Ref<CommandBuffer> p_cmd_buffer);
    void command_buffer_end(Ref<CommandBuffer> p_cmd_buffer);
    void command_buffer_execute_secondary(Ref<CommandBuffer> p_cmd_buffer,
                                          Vector<Ref<CommandBuffer>> p_secondary_cmd_buffers);

    /* render pipeline */
public:
    PipelineID render_pipeline_create(Ref<ShaderProgram> p_shader_program,
                                      VkPrimitiveTopology p_render_primitive,
                                      VkPipelineVertexInputStateCreateInfo vertex_input,
                                      PipelineRasterizationState p_rasterization_state,
                                      PipelineMultisampleState p_multisample_state,
                                      PipelineDepthStencilState p_depth_stencil_state,
                                      PipelineColorBlendState p_blend_state,
                                      Vector<int32_t> p_color_attachments,
                                      Vector<VkDynamicState> p_dynamic_state,
                                      VkRenderPass p_render_pass, uint32_t p_render_subpass);

    /* commands */
    void command_begin_render_pass(Ref<CommandBuffer> p_cmd_buffer, VkRenderPass p_render_pass,
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

    // binding
    void command_bind_render_pipeline(Ref<CommandBuffer> p_cmd_buffer, PipelineID p_pipeline);
    void command_bind_render_uniform_sets(Ref<CommandBuffer> p_cmd_buffer,
                                          Vector<Ref<UniformSet>> p_uniforms,
                                          Ref<ShaderProgram> p_program, uint32_t p_first_set_index,
                                          uint32_t p_set_count);

    // drawing
    void command_render_draw(Ref<CommandBuffer> p_cmd_buffer, uint32_t p_vertex_count,
                             uint32_t p_instance_count, uint32_t p_base_vertex,
                             uint32_t p_first_instance);
    void command_render_draw_indexed(Ref<CommandBuffer> p_cmd_buffer, uint32_t p_index_count,
                                     uint32_t p_instance_count, uint32_t p_first_vertex,
                                     int32_t p_vertex_offset, uint32_t p_first_instance);

    // buffer binding
    void command_render_bind_vertex_buffers(Ref<CommandBuffer> p_cmd_buffer,
                                            uint32_t p_binding_count, Ref<Buffer> p_buffer,
                                            uint64_t p_offset);
    void command_render_bind_index_buffer(Ref<CommandBuffer> p_cmd_buffer, Ref<Buffer> p_buffer,
                                          VkIndexType p_index_type, uint64_t p_offset);

    // dynamic state
    void command_render_set_blend_constants(Ref<CommandBuffer> p_cmd_buffer,
                                            float *p_blend_constants);
    void command_render_set_line_width(Ref<CommandBuffer> p_cmd_buffer, float p_width);

    /* compute pipeline */
    PipelineID compute_pipeline_create(const char *p_program_name);

    void command_bind_compute_pipeline(Ref<CommandBuffer> p_cmd_buffer, PipelineID p_pipeline);
    void command_bind_compute_uniform_sets(Ref<CommandBuffer> p_cmd_buffer,
                                           Vector<UniformSet> p_uniform_sets);
    void command_compute_dispatch(Ref<CommandBuffer> p_cmd_buffer, uint32_t p_x_groups,
                                  uint32_t p_y_groups, uint32_t p_z_groups);

public:
    RenderingDeviceDriver(RenderingDriverContext *p_context_driver);
    ~RenderingDeviceDriver();
};

using RDD = RenderingDeviceDriver;
