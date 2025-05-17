#pragma once

#pragma region forward declarations
#include <cstdint>


#include "../PerThreadRenderContext.h"
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
    static TextureMetaData createImageKTX(PerThreadRenderContext rendererContext, const char* path, TextureType type, bool mips,
                                          VkSamplerAddressMode mode);
    static VkImageView createTextureImageView(PerThreadRenderContext rendererContext, TextureMetaData data, VkImageViewType type);
    void createTextureSampler(VkSampler* textureSampler, PerThreadRenderContext rendererContext, VkSamplerAddressMode mode,
                              float bias, uint32_t maxMip, bool shadow = false);
    void createDepthPyramidSampler(VkSampler* textureSampler, PerThreadRenderContext rendererContext, uint32_t maxMip);

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
    TextureCreationInfoArgs MakeCreationArgsFromFilepathArgs(const char* path, TextureType type,
                                                             VkImageViewType viewType = static_cast<VkImageViewType>(-1));

    TextureCreationInfoArgs MakeTextureCreationArgsFromGLTFArgs(const char* OUTPUT_PATH,
                                                                VkFormat format, VkSamplerAddressMode samplerMode, unsigned char* pixels, uint64_t width,
                                                                uint64_t height, int mipCt, bool compress);

    TextureCreationInfoArgs MakeTextureCreationArgsFromCachedGLTFArgs(const char* OUTPUT_PATH,
                                                                      VkSamplerAddressMode samplerMode);

    TextureCreationStep1Result CreateTextureFromArgs_Start(PerThreadRenderContext context, TextureCreationInfoArgs a);
    TextureData CreateTextureFromArgsFinalize(PerThreadRenderContext outputTextureOwnerContext, TextureCreationStep1Result startResult);
}