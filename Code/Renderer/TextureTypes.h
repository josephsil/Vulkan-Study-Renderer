#pragma once
#include <Renderer/TextureTypes.h>
#include <Renderer/VulkanIncludes/forward-declarations-renderer.h>

struct textureFormatInfo
{
    VkFormat format;
    uint32_t width;
    uint32_t height;
    uint32_t mipCt;
    uint32_t layerCt;
};

struct VkImageAndMemory
{
    VkImage image;
    VkDeviceMemory memory;
};

struct TextureMetaData
{
    VkImageAndMemory textureImage;
    textureFormatInfo dimensionsInfo;
    VkDeviceMemory ktxMemory;
    VkSamplerAddressMode mode;
};


struct TextureData
{
    VkDescriptorImageInfo vkImageInfo; //Info necessary for rendering
    TextureMetaData metaData; //Other metadata for the renderer/engine to use someday
};