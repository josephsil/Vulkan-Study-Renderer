#pragma once

#pragma region forward declarations
#include <cstdint>


#include "../RendererContext.h"
#include  "../VulkanIncludes/Vulkan_Includes.h"
using ktxTexture2 = struct ktxTexture2;
using ktxVulkanTexture = struct ktxVulkanTexture;

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

namespace TextureCreation
{
    static TextureMetaData createImageKTX(RendererContext rendererContext, const char* path, TextureType type, bool mips,
                                          VkSamplerAddressMode mode);
    static VkImageView createTextureImageView(RendererContext rendererContext, TextureMetaData data, VkImageViewType type);
    void createTextureSampler(VkSampler* textureSampler, RendererContext rendererContext, VkSamplerAddressMode mode,
                              float bias, uint32_t maxMip, bool shadow = false);
    void createDepthPyramidSampler(VkSampler* textureSampler, RendererContext rendererContext, uint32_t maxMip);

    enum class TextureCreationMode
    {
        FILE,
        GLTFCREATE,
        GLTFCACHED
    };
    struct FILE_addtlargs
    {
        char* path;
        TextureType type;
        VkImageViewType viewType;
    };
    struct GLTFCREATE_addtlargs
    {
        char* OUTPUT_PATH;
        VkFormat format;
        VkSamplerAddressMode samplerMode;
        unsigned char* pixels;
        uint64_t width;
        uint64_t height;
        int mipCt;
        bool compress;
    };
    struct GLTFCACHE_addtlargs
    {
        char* OUTPUT_PATH;
        VkSamplerAddressMode samplerMode;
    };
  
    struct TextureCreationInfoArgs
    {
        RendererContext ctx;
        TextureCreationMode mode;
        union TextureCreationModeArgs {
            FILE_addtlargs fileArgs;
            GLTFCREATE_addtlargs gltfCreateArgs;
            GLTFCACHE_addtlargs gltfCacheArgs;
        }args;
    };

    struct TextureCreationStep1Result
    {
        TextureCreationMode mode;
        TextureMetaData metaData;
        TextureType type;
        VkImageViewType viewType;
    };
    TextureCreationInfoArgs MakeCreationArgsFromFilepathArgs(RendererContext rendererContext , const char* path, TextureType type,
                              VkImageViewType viewType = static_cast<VkImageViewType>(-1));

    TextureCreationInfoArgs MakeTextureCreationArgsFromGLTFArgs(RendererContext rendererContext ,  const char* OUTPUT_PATH,
                                      VkFormat format, VkSamplerAddressMode samplerMode, unsigned char* pixels, uint64_t width,
                                      uint64_t height, int mipCt, CommandBufferPoolQueue* commandbuffer, bool compress);

    TextureCreationInfoArgs MakeTextureCreationArgsFromCachedGLTFArgs(RendererContext rendererContext ,  const char* OUTPUT_PATH,
                                 VkSamplerAddressMode samplerMode,
                                 CommandBufferPoolQueue* commandbuffer);

    TextureCreationStep1Result CreateTextureFromArgs_Start(TextureCreationInfoArgs a);
    TextureData CreateTextureFromArgsFinalize(RendererContext outputTextureOwnerContext, TextureCreationStep1Result startResult);
    TextureData CreateTextureFromArgs(TextureCreationInfoArgs a);
}