#pragma once

#pragma region forward declarations
#include <cstdint>

#include "BufferAndPool.h"
#include "RendererContext.h"
#include  "VulkanIncludes/Vulkan_Includes.h"
using ktxTexture2 =  struct ktxTexture2;
using  ktxVulkanTexture = struct ktxVulkanTexture;

#pragma endregion
enum TextureType
{
    DIFFUSE,
    SPECULAR,
    NORMAL,
    CUBE,
    LINEAR_DATA,
    DATA_DONT_COMPRESS
};

struct textureFormatInfo
{
    VkFormat format;
    VkImageType type;
    uint64_t width;
    uint64_t  height;
    uint64_t  mipCt;
    uint64_t layerCt;
};
struct TextureMetaData
{
    VkImage textureImage;
    textureFormatInfo dimensionsInfo;
    VkDeviceMemory ktxMemory;
    
};



struct TextureData
{
    VkDescriptorImageInfo vkImageInfo; //Info necessary for rendering
    TextureMetaData metaData;           //Other metadata for the renderer/engine to use someday
};

static TextureMetaData createImageKTX(RendererContext rendererContext, const char* path, TextureType type, bool mips,
                               bool useExistingBuffer = false, bufferAndPool* buffer = nullptr);
static VkImageView createTextureImageView(RendererContext rendererContext, TextureMetaData data, VkImageViewType type);
void createTextureSampler(VkSampler* textureSampler, RendererContext rendererContext, VkSamplerAddressMode mode, float bias, uint32_t maxMip, bool shadow = false);
void createDepthPyramidSampler(VkSampler* textureSampler, RendererContext rendererContext, uint32_t maxMip);
//FILEPATH PATH 
TextureData createTexture(RendererContext rendererContext, const char* path, TextureType type, VkImageViewType viewType = (VkImageViewType)-1);
//GLTF PATH 
TextureData createTexture(RendererContext rendererContext, const char* OUTPUT_PATH, const char* textureName,  VkFormat format,  VkSamplerAddressMode samplerMode, unsigned char* pixels, uint64_t width, uint64_t height, int mipCt, bufferAndPool commandbuffer, bool compress);
TextureData createTexture(RendererContext rendererContext, const char* OUTPUT_PATH, const char* textureName, VkFormat format, VkSamplerAddressMode samplerMode,
            uint64_t width, uint64_t height, int mipCt, bufferAndPool commandbuffer);


