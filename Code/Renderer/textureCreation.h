#pragma once
#include <cstdint>
#include "VulkanIncludes/forward-declarations-renderer.h"

struct BufferCreationContext;
struct RendererDeletionQueue;
struct CommandPoolManager;

namespace TextureUtilities
{

    //TODO JS: this -1 stuff is sketchy
    VkImageView createImageView(BufferCreationContext rendererContext,
                                VkImage image, VkFormat format,
                                VkImageAspectFlags aspectFlags = -1, VkImageViewType type = (VkImageViewType)-1,
                                uint32_t miplevels = 1, uint32_t layerCount = 1, uint32_t layer = 0);

    void createImage(BufferCreationContext rendererContext, uint64_t width, uint64_t height, VkFormat format,
                     VkImageTiling tiling,
                     VkFlags usage, VkFlags properties, VkImage& image,
                     VmaAllocation& allocation, uint32_t miplevels = 1, uint32_t araryLayers = 1, bool cubeCompatible = false, const char* debugName = "CreateImage image");

    void transitionImageLayout(BufferCreationContext rendererContext, VkImage image, VkFormat format, VkImageLayout oldLayout,
                               VkImageLayout newLayout, VkCommandBuffer workingBuffer,
                               uint32_t miplevels = 1, bool useTransferPool = true, bool depth = false);

    void generateMipmaps(BufferCreationContext rendererContext, VkImage image, VkFormat imageFormat, int32_t texWidth,
                         int32_t texHeight, uint32_t mipLevels);
    void copyBufferToImage(CommandPoolManager* commandPoolManager, VkBuffer buffer, VkImage image, uint32_t width,
                           uint32_t height,
                           VkCommandBuffer workingBuffer = nullptr);
}

