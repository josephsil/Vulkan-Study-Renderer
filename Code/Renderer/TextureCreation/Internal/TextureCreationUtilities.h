#pragma once
#include <cstdint>

#include <Renderer/VulkanIncludes/forward-declarations-renderer.h>

struct BufferCreationContext;
class RendererDeletionQueue;
class CommandPoolManager;

namespace TextureUtilities
{
    VkImageView createImageViewCustomMip(BufferCreationContext rendererContext, VkImage image, VkFormat format,
                                         VkImageAspectFlags aspectFlags,
                                         VkImageViewType type, uint32_t layerCount, uint32_t layer, uint32_t mipCount,
                                         uint32_t baseMip);


    VkImageView createImageView(BufferCreationContext handles,
                                VkImage image, VkFormat format,
                                VkImageAspectFlags aspectFlags, VkImageViewType type,
                                uint32_t layerCount, uint32_t layer);

    void createImage(BufferCreationContext rendererHandles, uint64_t width, uint64_t height, VkFormat format,
                     VkImageTiling tiling,
                     VkFlags usage, VkFlags properties, VkImage& image,
                     VmaAllocation& allocation, uint32_t miplevels = 1, uint32_t araryLayers = 1,
                     bool cubeCompatible = false, const char* debugName = "CreateImage image");

    void transitionImageLayout(BufferCreationContext rendererHandles, VkImage image, VkFormat format,
                               VkImageLayout oldLayout,
                               VkImageLayout newLayout, VkCommandBuffer workingBuffer,
                               uint32_t miplevels = 1, bool useTransferPool = true, bool depth = false);

    void generateMipmaps(BufferCreationContext rendererHandles, VkImage image, VkFormat imageFormat, int32_t texWidth,
                         int32_t texHeight, uint32_t mipLevels);
    void copyBufferToImage(CommandPoolManager* commandPoolManager, VkBuffer buffer, VkImage image, uint32_t width,
                           uint32_t height,
                           VkCommandBuffer workingBuffer = nullptr);
}
