#pragma once
#ifndef  VULKAN_INCLUDES
#define VULKAN_INCLUDES 
#include <vulkan/vulkan_core.h>
#endif
#define VK_CHECK(call) \
do { \
VkResult result_ = call; \
assert(result_ == VK_SUCCESS); \
} while (0)

struct RenderTextureFormat
{
    VkFormat ColorFormat;
    VkFormat DepthFormat;
};

struct bufferAndPool
{
    VkCommandBuffer buffer;
    VkCommandPool pool;
    VkQueue queue;
};
