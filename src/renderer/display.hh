#pragma once

#include <volk.h>

enum VSyncMode { VSYNC_DISABLED, VSYNC_ENABLED, VSYNC_ADAPTIVE, VSYNC_MAILBOX };

struct Surface {
    VkSurfaceKHR vk_surface = VK_NULL_HANDLE;
    uint32_t width = 0;
    uint32_t height = 0;
    VSyncMode vsync_mode = VSYNC_ENABLED;
    bool needs_resize = false;
    bool hdr_output = false;
};
