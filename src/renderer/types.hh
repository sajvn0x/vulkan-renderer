#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>

#include "core/containers.hh"
#include "core/traits.hh"
#include "core/typedefs.hh"
#include "vma.hh"

/* driver context */
struct VulkanApiVersion {
    uint32_t api_version = VK_API_VERSION_1_2;

    VulkanApiVersion(uint32_t p_api_version) : api_version(p_api_version) {}
    _ALWAYS_INLINE_ bool supports(uint32_t version) const { return api_version >= version; }
    _ALWAYS_INLINE_ bool supports_vulkan_1_2() const { return supports(VK_API_VERSION_1_2); }
    _ALWAYS_INLINE_ bool supports_vulkan_1_3() const { return supports(VK_API_VERSION_1_3); }
    _ALWAYS_INLINE_ bool supports_vulkan_1_4() const { return supports(VK_API_VERSION_1_4); }
};

struct Vendor {
    constexpr static uint32_t VENDOR_UNKNOWN = 0x0;
    constexpr static uint32_t VENDOR_AMD = 0x1002;
    constexpr static uint32_t VENDOR_IMGTEC = 0x1010;
    constexpr static uint32_t VENDOR_APPLE = 0x106B;
    constexpr static uint32_t VENDOR_NVIDIA = 0x10DE;
    constexpr static uint32_t VENDOR_ARM = 0x13B5;
    constexpr static uint32_t VENDOR_MICROSOFT = 0x1414;
    constexpr static uint32_t VENDOR_QUALCOMM = 0x5143;
    constexpr static uint32_t VENDOR_INTEL = 0x8086;
};

enum DeviceType {
    DEVICE_TYPE_OTHER = 0x0,
    DEVICE_TYPE_INTEGRATED_GPU = 0x1,
    DEVICE_TYPE_DISCRETE_GPU = 0x2,
    DEVICE_TYPE_VIRTUAL_GPU = 0x3,
    DEVICE_TYPE_CPU = 0x4,
    DEVICE_TYPE_MAX = 0x5
};

struct Device {
    String name = "Unknown";
    uint32_t vendor = Vendor::VENDOR_UNKNOWN;
    DeviceType type = DEVICE_TYPE_OTHER;
};

/* driver device */
struct Size2i {
    float x, y;
};

// capabilities
struct SubgroupCapabilities {
    uint32_t size = 0;
    uint32_t min_size = 0;
    uint32_t max_size = 0;
    VkShaderStageFlags supported_stages = 0;

    VkSubgroupFeatureFlags supported_operations = 0;
    VkBool32 quad_operations_in_all_stages = false;
    bool size_control_is_supported = false;

    uint32_t supported_stages_flags_rd() const;
    String supported_stages_desc() const;
    uint32_t supported_operations_flags_rd() const;
    String supported_operations_desc() const;
};

struct ShaderCapabilities {
    bool shader_float16_is_supported = false;
    bool shader_int8_is_supported = false;
};

struct StorageBufferCapabilities {
    bool storage_buffer_16_bit_access_is_supported = false;
    bool uniform_and_storage_buffer_16_bit_access_is_supported = false;
    bool storage_push_constant_16_is_supported = false;
    bool storage_input_output_16 = false;
};

struct MultiviewCapabilities {
    bool is_supported = false;
    bool geometry_shader_is_supported = false;
    bool tessellation_shader_is_supported = false;
    uint32_t max_view_count = 0;
    uint32_t max_instance_count = 0;
};

struct FragmentShadingRateCapabilities {
    Size2i min_texel_size;
    Size2i max_texel_size;
    Size2i max_fragment_size;
    bool pipeline_supported = false;
    bool primitive_supported = false;
    bool attachment_supported = false;
};

struct FragmentDensityMapCapabilities {
    Size2i min_texel_size;
    Size2i max_texel_size;
    Size2i offset_granularity;
    bool attachment_supported = false;
    bool dynamic_attachment_supported = false;
    bool non_subsampled_images_supported = false;
    bool invocations_supported = false;
    bool offset_supported = false;
};

/* buffers */
struct Buffer : public RefTarget<Buffer> {
    VkBuffer vk_buffer = VK_NULL_HANDLE;
    struct {
        VmaAllocation handle = nullptr;
        uint64_t size = UINT64_MAX;
    } allocation;
    uint64_t size = 0;
    VkBufferUsageFlags usage = 0;
    VkBufferView vk_view = VK_NULL_HANDLE;
    void *mapped_data = nullptr;
};

/* texture */
struct TextureView {
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkComponentSwizzle swizzle_r = VK_COMPONENT_SWIZZLE_R;
    VkComponentSwizzle swizzle_g = VK_COMPONENT_SWIZZLE_G;
    VkComponentSwizzle swizzle_b = VK_COMPONENT_SWIZZLE_B;
    VkComponentSwizzle swizzle_a = VK_COMPONENT_SWIZZLE_A;
};

struct TextureFormat {
    VkFormat format = VK_FORMAT_UNDEFINED;
    uint32_t width = 1;
    uint32_t height = 1;
    uint32_t depth = 1;
    uint32_t array_layers = 1;
    uint32_t mipmaps = 1;
    VkImageType texture_type = VK_IMAGE_TYPE_2D;
    VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
    uint32_t usage_bits = 0;
    bool is_resolve_buffer = false;
    bool is_discardable = false;
    bool is_subsampled = false;

    bool operator==(const TextureFormat &b) const {
        if (format != b.format) {
            return false;
        } else if (width != b.width) {
            return false;
        } else if (height != b.height) {
            return false;
        } else if (depth != b.depth) {
            return false;
        } else if (array_layers != b.array_layers) {
            return false;
        } else if (mipmaps != b.mipmaps) {
            return false;
        } else if (texture_type != b.texture_type) {
            return false;
        } else if (samples != b.samples) {
            return false;
        } else if (usage_bits != b.usage_bits) {
            return false;
        } else if (is_resolve_buffer != b.is_resolve_buffer) {
            return false;
        } else if (is_discardable != b.is_discardable) {
            return false;
        } else {
            return true;
        }
    }
};

struct Texture : RefTarget<Texture> {
    VkImage vk_image = VK_NULL_HANDLE;
    VkImageView vk_image_view = VK_NULL_HANDLE;
    VkFormat vk_format = VK_FORMAT_UNDEFINED;
    VkImageCreateInfo vk_create_info = {};
    VkImageViewCreateInfo vk_image_create_info = {};
    struct {
        VmaAllocation handle = nullptr;
        VmaAllocationInfo info = {};
    } allocation;
    bool is_subsampled = false;
};

/* sampler */
struct Sampler : RefTarget<Sampler> {
    VkSampler handle = VK_NULL_HANDLE;
};

struct SamplerState {
    VkFilter mag_filter = VK_FILTER_LINEAR;
    VkFilter min_filter = VK_FILTER_LINEAR;
    VkSamplerMipmapMode mip_filter = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    VkSamplerAddressMode repeat_u = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    VkSamplerAddressMode repeat_v = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    VkSamplerAddressMode repeat_w = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    float lod_bias = 0.0f;
    bool use_anisotropy = false;
    float anisotropy_max = 1.0f;
    bool enable_compare = false;
    VkCompareOp compare_op = VK_COMPARE_OP_ALWAYS;
    float min_lod = 0.0f;
    float max_lod = 1e20;
    VkBorderColor border_color = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    bool unnormalized_uvw = false;
};

/* fence */
struct Fence : RefTarget<Fence> {
    VkFence vk_fence = VK_NULL_HANDLE;
};

/* semaphore */
struct Semaphore : RefTarget<Semaphore> {
    VkSemaphore vk_semaphore = VK_NULL_HANDLE;
};
