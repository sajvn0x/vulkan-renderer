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
    BufferID buffer_create(uint64_t p_size, VkBufferUsageFlags p_usage,
                           MemoryAllocationType p_allocation_type);
    bool buffer_set_texel_format(BufferID p_buffer, VkFormat p_format);
    void buffer_free(BufferID p_buffer, VkFormat p_format);
    uint64_t buffer_get_allocation_size(BufferID p_buffer);
    uint8_t *buffer_map(BufferID p_buffer);
    void buffer_unmap(BufferID p_buffer);
    uint64_t buffer_get_device_address(BufferID p_buffer);

    /* texture */
private:
    VkSampleCountFlagBits _ensure_supported_sample_count(
        VkSampleCountFlagBits p_requested_sample_count);

public:
    TextureID texture_create(const TextureFormat &p_format, const TextureView &p_view);
    void texture_free(TextureID p_texture);

    /* sampler */
public:
    SamplerID sampler_create(const SamplerState &p_state);
    void sampler_free(SamplerID p_sampler);
    bool sampler_is_format_supported_for_filter(VkFormat p_format, VkFilter p_filter);

    /* framebuffer */
public:
    FramebufferID framebuffer_create(RenderPassID p_render_pass, Vector<TextureID> p_attachments,
                                     uint32_t p_width, uint32_t p_height);
    void framebuffer_free(FramebufferID p_framebuffer);

    /* render pass */
public:
    RenderPassID render_pass_create(Vector<Attachment> &p_attachments, Vector<Subpass> &p_subpasses,
                                    Vector<SubpassDependency> &p_subpass_dependencies);
    void render_pass_free(RenderPassID p_render_pass);

    // dynamic rendering
    void command_render_begin_rendering(CommandBufferID p_cmd_buffer, VkRect2D p_render_area);
    void command_render_end_rendering(CommandBufferID p_cmd_buffer);

public:
    /* swapchain */
    SwapchainID swapchain_create(Surface &p_surface);
    Error swapchain_resize(SwapchainID p_old_swapchain = SwapchainID());
    void swapchain_acquire_next_image(SwapchainID p_swapchain, SemaphoreID p_semaphore);
    FramebufferID swapchain_acquire_framebuffer();
    bool _determine_swap_chain_format(Surface &p_surface, VkFormat &p_format,
                                      VkColorSpaceKHR &p_color_space);
    void swapchain_free(SwapchainID p_swapchain);

    /* command queue */
private:
    uint32_t command_queue_family_get();

public:
    CommandQueueID command_queue_create(int p_cmd_queue_family, uint32_t p_image_count);
    Error command_queue_execute_and_present(CommandQueueID p_cmd_queue, FenceID p_fence,
                                            SwapchainID p_swapchain, CommandBufferID p_cmd_buffer,
                                            uint32_t p_frame_index);
    void command_queue_free(CommandQueueID p_cmd_queue);

    /* uniform sets */
private:
    static const uint32_t MAX_UNIFORM_POOL_ELEMENT = 65535;

    using DescriptorSetPools = HashMap<uint32_t, VkDescriptorPool>;
    DescriptorSetPools descriptor_set_pools;
    uint32_t max_descriptor_sets_per_pool = 100;

    VkDescriptorPool _descriptor_set_pool_create(const VkDescriptorType &p_key);
    void _descriptor_set_pool_unreference(VkDescriptorPool p_vk_descriptor_pool);

public:
    UniformSetID uniform_set_create(Vector<BoundUniform> p_uniforms, Ref<ShaderProgram> p_shader,
                                    uint32_t p_set_index);
    void uniform_set_free(UniformSetID p_uniform_set);

    /* commands */
    CommandPoolID command_pool_create(uint32_t queue_family_index,
                                      VkCommandBufferLevel cmd_buffer_level);
    bool command_pool_reset(CommandPoolID p_cmd_pool);
    void command_pool_free(CommandPoolID p_cmd_pool);

    CommandBufferID command_buffer_create(CommandPoolID p_cmd_pool);
    bool command_buffer_begin(CommandBufferID p_cmd_buffer);
    bool command_buffer_begin_secondary(CommandBufferID p_cmd_buffer);
    void command_buffer_end(CommandBufferID p_cmd_buffer);
    void command_buffer_execute_secondary(CommandBufferID p_cmd_buffer,
                                          Vector<CommandBufferID> p_secondary_cmd_buffers);

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
    void command_begin_render_pass(CommandBufferID p_cmd_buffer, VkRenderPass p_render_pass,
                                   FramebufferID p_framebuffer, VkCommandBufferLevel p_cmd_level,
                                   const VkRect2D &p_rect, Vector<VkClearValue> p_clear_values);
    void command_end_render_pass(CommandBufferID p_cmd_buffer);
    void command_next_render_subpass(CommandBufferID p_cmd_buffer,
                                     VkCommandBufferLevel p_cmd_level);
    void command_render_set_viewport(CommandBufferID p_cmd_buffer, Vector<VkRect2D> p_viewports);
    void command_render_set_scissor(CommandBufferID p_cmd_buffer, Vector<VkRect2D> p_scissors);
    void command_render_clear_attachments(CommandBufferID p_cmd_buffer,
                                          Vector<VkClearAttachment> p_attachment_clears,
                                          Vector<VkRect2D> p_rects);

    // binding
    void command_bind_render_pipeline(CommandBufferID p_cmd_buffer, PipelineID p_pipeline);
    void command_bind_render_uniform_sets(CommandBufferID p_cmd_buffer,
                                          Vector<UniformSetID> p_uniforms,
                                          Ref<ShaderProgram> p_program, uint32_t p_first_set_index,
                                          uint32_t p_set_count);

    // drawing
    void command_render_draw(CommandBufferID p_cmd_buffer, uint32_t p_vertex_count,
                             uint32_t p_instance_count, uint32_t p_base_vertex,
                             uint32_t p_first_instance);
    void command_render_draw_indexed(CommandBufferID p_cmd_buffer, uint32_t p_index_count,
                                     uint32_t p_instance_count, uint32_t p_first_vertex,
                                     int32_t p_vertex_offset, uint32_t p_first_instance);

    // buffer binding
    void command_render_bind_vertex_buffers(CommandBufferID p_cmd_buffer, uint32_t p_binding_count,
                                            BufferID p_buffer, uint64_t p_offset);
    void command_render_bind_index_buffer(CommandBufferID p_cmd_buffer, BufferID p_buffer,
                                          VkIndexType p_index_type, uint64_t p_offset);

    // dynamic state
    void command_render_set_blend_constants(CommandBufferID p_cmd_buffer, float *p_blend_constants);
    void command_render_set_line_width(CommandBufferID p_cmd_buffer, float p_width);

    /* compute pipeline */
    PipelineID compute_pipeline_create(const char *p_program_name);

    void command_bind_compute_pipeline(CommandBufferID p_cmd_buffer, PipelineID p_pipeline);
    void command_bind_compute_uniform_sets(CommandBufferID p_cmd_buffer,
                                           Vector<UniformSet> p_uniform_sets);
    void command_compute_dispatch(CommandBufferID p_cmd_buffer, uint32_t p_x_groups,
                                  uint32_t p_y_groups, uint32_t p_z_groups);

public:
    RenderingDeviceDriver(RenderingDriverContext *p_context_driver);
    ~RenderingDeviceDriver();
};

using RDD = RenderingDeviceDriver;
