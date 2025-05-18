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
    static TextureMetaData CreateImageFromCachedKTX(PerThreadRenderContext rendererContext, const char* path, TextureType type, bool mips,
                                          VkSamplerAddressMode mode);
    static VkImageView createTextureImageView(PerThreadRenderContext rendererContext, TextureMetaData data, VkImageViewType type);
    void createTextureSampler(VkSampler* textureSampler, PerThreadRenderContext rendererContext, VkSamplerAddressMode mode,
                              float bias, uint32_t maxMip, bool shadow = false);
    void createDepthPyramidSampler(VkSampler* textureSampler, PerThreadRenderContext rendererContext, uint32_t maxMip);

    enum class TextureCreationMode
    {
        FILE,
        GLTFCREATE,
        KTXCACHED
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
    struct KTX_CACHE_addtlargs
    {
        char* cachedKtxPath;
        VkSamplerAddressMode samplerMode;
        bool isCube;
    };
  
    struct TextureCreationInfoArgs
    {
        TextureCreationMode mode;
        union TextureCreationModeArgs {
            FILE_addtlargs fileArgs;
            GLTFCREATE_addtlargs gltfCreateArgs;
            KTX_CACHE_addtlargs gltfCacheArgs;
        }args;
    };

    struct TextureCreationStep1Result
    {
        TextureCreationMode mode;
        TextureMetaData metaData;
        VkImageViewType viewType;
    };

    //Everywhere in the actual renderer we use ktx textures, but sometimes we load other textures from disk before caching them to ktx
    //These structs are used for those temporary import textures. Need a better name / home
    struct stagingTextureData
    {
        VkImage textureImage;
        VmaAllocation alloc;
        uint64_t width;
        uint64_t height;
        uint64_t mipCt;
    };
    
    struct TempTextureStepResult
    {
        stagingTextureData stagingTexture;
        char* outputPath;
        VkFormat format;
        bool compress;
        VkSamplerAddressMode samplerMode;
        TextureType type;
        VkImageViewType viewType;
        
    };

    
    TextureCreationInfoArgs MakeCreationArgsFromFilepathArgs(const char* path,Allocator arena, TextureType type,
                                                             VkImageViewType viewType = static_cast<VkImageViewType>(-1));

    TextureCreationInfoArgs MakeTextureCreationArgsFromGLTFArgs(const char* OUTPUT_PATH,
                                                                VkFormat format, VkSamplerAddressMode samplerMode, unsigned char* pixels, uint64_t width,
                                                                uint64_t height, int mipCt, bool compress);

    TextureCreationInfoArgs MakeTextureCreationArgsFromCachedKTX(const char* cachedFilePath,
                                                                      VkSamplerAddressMode samplerMode, bool isCube = false, bool noCompress = false);

    TextureCreation::TempTextureStepResult CreateTextureFromArgs_Start(PerThreadRenderContext context,
                                                                       TextureCreationInfoArgs a);
    TextureCreationStep1Result
CreateTextureFromArgs_LoadCachedFiles(PerThreadRenderContext context, TextureCreation::TextureCreationInfoArgs a);
    TextureData CreateTextureFromArgsFinalize(PerThreadRenderContext outputTextureOwnerContext, TextureCreationStep1Result startResult);

    TextureCreation::TextureCreationInfoArgs CreateTexture_Cache_Temp_To_KTX_Step(PerThreadRenderContext rendererContext, TextureCreation::TempTextureStepResult r);
}