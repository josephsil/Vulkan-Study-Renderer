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

    enum class TextureImportMode
    {
        IMPORT_FILE,
        IMPORT_GLTF,
        LOAD_KTX_CACHED
    };
    struct IMPORT_FILE_args
    {
        char* path;
        TextureType type;
        VkImageViewType viewType;
    };
    struct IMPORT_GLTF_args
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
    struct LOAD_KTX_CACHED_args
    {
        char* cachedKtxPath;
        VkSamplerAddressMode samplerMode;
        bool isCube;
    };
  
    struct TextureImportRequest
    {
        TextureImportMode mode;
        union TextureLoadingModeData {
            IMPORT_FILE_args fileArgs;
            IMPORT_GLTF_args gltfCreateArgs;
            LOAD_KTX_CACHED_args gltfCacheArgs;
        }addtlData;
    };


    //Imported texture, ready to be passed to the renderer 
    struct TextureImportResult
    {
        TextureImportMode mode;
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
    
    struct TextureImportProcessTemporaryTexture
    {
        stagingTextureData stagingTexture;
        char* outputPath;
        VkFormat format;
        bool compress;
        VkSamplerAddressMode samplerMode;
        TextureType type;
        VkImageViewType viewType;
        
    };

    
    TextureImportRequest MakeCreationArgsFromFilepathArgs(const char* path,Allocator arena, TextureType type,
                                                             VkImageViewType viewType = static_cast<VkImageViewType>(-1));

    TextureImportRequest MakeTextureCreationArgsFromGLTFArgs(const char* OUTPUT_PATH,
                                                                VkFormat format, VkSamplerAddressMode samplerMode, unsigned char* pixels, uint64_t width,
                                                                uint64_t height, int mipCt, bool compress);

    TextureImportRequest MakeTextureCreationArgsFromCachedKTX(const char* cachedFilePath,
                                                                      VkSamplerAddressMode samplerMode, bool isCube = false, bool noCompress = false);

    TextureCreation::TextureImportProcessTemporaryTexture CreateTextureFromArgs_Start(PerThreadRenderContext context,
                                                                       TextureImportRequest a);
    TextureImportResult
CreateTextureFromArgs_LoadCachedFiles(PerThreadRenderContext context, TextureCreation::LOAD_KTX_CACHED_args importArguments);
    TextureData CreateTextureFromArgsFinalize(PerThreadRenderContext outputTextureOwnerContext, TextureImportResult startResult);

    TextureCreation::LOAD_KTX_CACHED_args CreateTexture_Cache_Temp_To_KTX_Step(
        PerThreadRenderContext rendererContext, TextureCreation::TextureImportProcessTemporaryTexture r);
}