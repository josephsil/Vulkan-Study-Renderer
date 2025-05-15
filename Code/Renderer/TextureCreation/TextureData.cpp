#define DEBUG 
#include "TextureData.h"
#include <ImageLibraryImplementations.h>
#include <Renderer/vulkan-utilities.h>
#include <Renderer/VulkanIncludes/Vulkan_Includes.h>

#include <Renderer/VulkanIncludes/VulkanMemory.h>

#include <cassert>
#include <iostream>
#include <ktxvulkan.h>


#include <Renderer/VulkanBuffers/bufferCreation.h>
#include <Renderer/CommandPoolManager.h>
#include <Renderer/TextureCreation/internal/TextureCreationUtilities.h>
#include <General/FileCaching.h>

#include "Renderer/MainRenderer/VulkanRendererInternals/RendererHelpers.h"

//Everywhere in the actual renderer we use ktx textures, but sometimes we load other textures from disk before caching them to ktx
//These structs are used for those temporary import textures. Need a better name / home
struct nonKTXTextureInfo
{
    VkImage textureImage;
    VmaAllocation alloc;
    uint64_t width;
    uint64_t height;
    uint64_t mipCt;
};

static nonKTXTextureInfo createtempTextureFromPath(PerThreadRenderContext rendererContext,CommandBufferPoolQueue buffer, const char* path, VkFormat format,
                                                   bool mips);
static nonKTXTextureInfo createTextureImage(PerThreadRenderContext rendererContext, const unsigned char* pixels,
                                            uint64_t texWidth, uint64_t texHeight, VkFormat format, bool mips, CommandBufferPoolQueue workingTextureBuffer);
static void cacheKTXFromTempTexture(PerThreadRenderContext rendererContext, nonKTXTextureInfo tempTexture, const char* outpath,
                                    VkFormat format, TextureType textureType, bool use_mipmaps, bool compress);
TextureMetaData GetOrLoadTextureFromPath(PerThreadRenderContext rendererContext, const char* path, VkFormat format, VkSamplerAddressMode mode,
                                 TextureType textureType, bool use_mipmaps, bool compress);

// TextureData TextureCreation::CreateTexture_part1(RendererContext rendererContext, const char* path, TextureType type, VkImageViewType viewType)
// {
// }
TextureMetaData CreateTextureFromPath_Synchronously_Start(PerThreadRenderContext rendererContext, TextureCreation::FILE_addtlargs args)
{
    VkFormat inputFormat;
    VkSamplerAddressMode mode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    bool uncompressed = false;

    bool use_mipmaps = true;
    switch (args.type)
    {
    case DIFFUSE:
        {
            inputFormat = VK_FORMAT_R8G8B8A8_SRGB;
            break;
        }
    case SPECULAR:
        {
            inputFormat = VK_FORMAT_R8G8B8A8_UNORM;
            break;
        }
    case NORMAL:
        {
            inputFormat = VK_FORMAT_R8G8B8A8_UNORM;
            break;
        }
    case LINEAR_DATA:
        {
            inputFormat = VK_FORMAT_R8G8B8A8_UNORM;
            mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            use_mipmaps = false;
            break;
        }
    case CUBE:
        {
            inputFormat = VK_FORMAT_R8G8B8A8_UNORM;
            mode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            break;
        }
    case DATA_DONT_COMPRESS:
        {
            inputFormat = VK_FORMAT_R8G8B8A8_UNORM;
            mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            use_mipmaps = false;
            uncompressed = true;
        }
    }

    return GetOrLoadTextureFromPath(rendererContext, args.path, inputFormat, mode, args.type, use_mipmaps, !uncompressed);
}

TextureData CreateTextureFromPath_Finalize(PerThreadRenderContext rendererContext, TextureMetaData tData, TextureType type,
        VkImageViewType _viewType)
{
    auto viewType = _viewType;
    if (viewType == -1)
    {
        viewType = type == CUBE ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_2D;
    }
    auto textureImageView = TextureCreation::createTextureImageView(rendererContext, tData, viewType);
    VkSampler textureSampler = {};
    TextureCreation::createTextureSampler(&textureSampler, rendererContext, tData.mode, 0, tData.dimensionsInfo.mipCt);
    return TextureData 
      {
        .vkImageInfo = {
            .sampler = textureSampler,
            .imageView = textureImageView,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        },
        .metaData = {
            .textureImage = tData.textureImage,
            .dimensionsInfo = tData.dimensionsInfo,
            .ktxMemory = tData.ktxMemory
        }

      };
}

TextureCreation::TextureCreationInfoArgs TextureCreation::MakeCreationArgsFromFilepathArgs(
    const char* path, TextureType type, VkImageViewType viewType)
{
    TextureCreationInfoArgs args;
    args = TextureCreationInfoArgs{
        TextureCreationMode::FILE
    };
    args.args.fileArgs =
        FILE_addtlargs{
            .path = const_cast<char*>(path),
            .type = type,
            .viewType = viewType
        };
return args;
}

TextureCreation::TextureCreationInfoArgs TextureCreation::MakeTextureCreationArgsFromGLTFArgs(
    const char* OUTPUT_PATH, VkFormat format, VkSamplerAddressMode samplerMode, unsigned char* pixels, uint64_t width,
    uint64_t height, int mipCt, bool compress)
{
    TextureCreationInfoArgs args;
    args = TextureCreationInfoArgs{
        TextureCreationMode::GLTFCREATE
    };
    args.args.gltfCreateArgs =
        GLTFCREATE_addtlargs{
            .OUTPUT_PATH = const_cast<char*>(OUTPUT_PATH),
            .format = format,
            .samplerMode = samplerMode,
            .pixels = pixels,
            .width = width,
            .height = height,
            .mipCt = mipCt,
            .compress =  compress
        };
    return args;
}

TextureCreation::TextureCreationInfoArgs TextureCreation::MakeTextureCreationArgsFromCachedGLTFArgs(
     const char* OUTPUT_PATH, VkSamplerAddressMode samplerMode)
{
    TextureCreationInfoArgs args;
    args = TextureCreationInfoArgs{
        TextureCreationMode::GLTFCACHED
    };
    args.args.gltfCacheArgs =
        GLTFCACHE_addtlargs{
            .OUTPUT_PATH = const_cast<char*>(OUTPUT_PATH),
            .samplerMode = samplerMode,
        };
    return args;     
}


TextureMetaData CreateTextureNewGltfTexture_Start(PerThreadRenderContext rendererContext,  TextureCreation::GLTFCREATE_addtlargs args)
{
    auto bandp = rendererContext.textureCreationcommandPoolmanager->beginSingleTimeCommands(false);
    nonKTXTextureInfo staging = createTextureImage(rendererContext, args.pixels, args.width, args.height, args.format, true, bandp);

    //We need a fence between creating the texture and caching the ktx, because the ktx requires a read to host memory
    //I'd really rather go wide paralellizing the cache ktx, but I forgot how it worked when i started writing this. Someday I should break this up
    //I could also do the ktx thing on a background pool, and just return the texture I got from staging?
    rendererContext.textureCreationcommandPoolmanager->endSingleTimeCommands(bandp, true);

    cacheKTXFromTempTexture(rendererContext, staging, args.OUTPUT_PATH, args.format, DIFFUSE, args.mipCt != 0, args.compress);
    auto ktxResult = TextureCreation::createImageKTX(rendererContext, args.OUTPUT_PATH, DIFFUSE, true, args.samplerMode);
    return ktxResult;
}

TextureData CreateTextureNewGltfTexture_Finalize(PerThreadRenderContext rendererContext, TextureMetaData tData)
{
    VkImageView textureImageView = TextureCreation::createTextureImageView(rendererContext, tData, VK_IMAGE_VIEW_TYPE_2D);
    assert(textureImageView != VK_NULL_HANDLE);
    VkSampler textureSampler = {};
    TextureCreation::createTextureSampler(&textureSampler, rendererContext, tData.mode, 0, tData.dimensionsInfo.mipCt);

    return TextureData 
    {
        .vkImageInfo = {
            .sampler = textureSampler,
            .imageView = textureImageView,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        },
        .metaData = tData //TODO1 check
    };
}





//FROM CACHED GLTF
TextureMetaData GetCachedTexture_Start(PerThreadRenderContext rendererContext, TextureCreation::GLTFCACHE_addtlargs args)
{
    return  TextureCreation::createImageKTX(rendererContext, args.OUTPUT_PATH, DIFFUSE, true, args.samplerMode);
}
TextureData GetCachedTexture_Finalize(PerThreadRenderContext rendererContext, TextureMetaData tData)
{
    auto textureImageView = TextureCreation::createTextureImageView(rendererContext, tData, VK_IMAGE_VIEW_TYPE_2D);
    assert(textureImageView != VK_NULL_HANDLE);

    VkSampler textureSampler = {};
    TextureCreation::createTextureSampler(&textureSampler, rendererContext, tData.mode, 0, 1);

    return TextureData 
    {
        .vkImageInfo = {
            .sampler = textureSampler,
            .imageView = textureImageView,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        },
        .metaData = tData //TODO1
    };
}

//TODO JS: Lifted from gltfiblsampler -- replace
uint32_t getFormatSize(VkFormat f)
{
    switch (f)
    {
    // 1 BYTE
    case VK_FORMAT_R4G4_UNORM_PACK8:

    case VK_FORMAT_R8_UNORM:
    case VK_FORMAT_R8_SNORM:
    case VK_FORMAT_R8_UINT:
    case VK_FORMAT_R8_SINT:
    case VK_FORMAT_R8_SRGB:
        return 1u;

    // 2 BYTE
    case VK_FORMAT_R4G4B4A4_UNORM_PACK16:
    case VK_FORMAT_B4G4R4A4_UNORM_PACK16:
    case VK_FORMAT_R5G6B5_UNORM_PACK16:
    case VK_FORMAT_B5G6R5_UNORM_PACK16:
    case VK_FORMAT_R5G5B5A1_UNORM_PACK16:
    case VK_FORMAT_B5G5R5A1_UNORM_PACK16:
    case VK_FORMAT_A1R5G5B5_UNORM_PACK16:

    case VK_FORMAT_R8G8_UNORM:
    case VK_FORMAT_R8G8_SNORM:
    case VK_FORMAT_R8G8_UINT:
    case VK_FORMAT_R8G8_SINT:
    case VK_FORMAT_R8G8_SRGB:

    case VK_FORMAT_R16_UNORM:
    case VK_FORMAT_R16_SNORM:
    case VK_FORMAT_R16_UINT:
    case VK_FORMAT_R16_SINT:
    case VK_FORMAT_R16_SFLOAT:
        return 2u;

    // 3 BYTE
    case VK_FORMAT_R8G8B8_UNORM:
    case VK_FORMAT_R8G8B8_SNORM:
    case VK_FORMAT_R8G8B8_UINT:
    case VK_FORMAT_R8G8B8_SINT:
    case VK_FORMAT_R8G8B8_SRGB:

    case VK_FORMAT_B8G8R8_UNORM:
    case VK_FORMAT_B8G8R8_SNORM:
    case VK_FORMAT_B8G8R8_UINT:
    case VK_FORMAT_B8G8R8_SINT:
    case VK_FORMAT_B8G8R8_SRGB:
        return 3u;

    // 4 BYTE
    case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
    case VK_FORMAT_A2R10G10B10_SNORM_PACK32:

    case VK_FORMAT_A2R10G10B10_UINT_PACK32:
    case VK_FORMAT_A2R10G10B10_SINT_PACK32:
    case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
    case VK_FORMAT_A2B10G10R10_SNORM_PACK32:

    case VK_FORMAT_A2B10G10R10_UINT_PACK32:
    case VK_FORMAT_A2B10G10R10_SINT_PACK32:

    case VK_FORMAT_B10G11R11_UFLOAT_PACK32:

    case VK_FORMAT_R8G8B8A8_UNORM:
    case VK_FORMAT_R8G8B8A8_SNORM:
    case VK_FORMAT_R8G8B8A8_UINT:
    case VK_FORMAT_R8G8B8A8_SINT:
    case VK_FORMAT_R8G8B8A8_SRGB:

    case VK_FORMAT_B8G8R8A8_UNORM:
    case VK_FORMAT_B8G8R8A8_SNORM:
    case VK_FORMAT_B8G8R8A8_UINT:
    case VK_FORMAT_B8G8R8A8_SINT:
    case VK_FORMAT_B8G8R8A8_SRGB:

    case VK_FORMAT_R16G16_UNORM:
    case VK_FORMAT_R16G16_SNORM:
    case VK_FORMAT_R16G16_UINT:
    case VK_FORMAT_R16G16_SINT:
    case VK_FORMAT_R16G16_SFLOAT:

    case VK_FORMAT_R32_UINT:
    case VK_FORMAT_R32_SINT:
    case VK_FORMAT_R32_SFLOAT:
        return 4u;

    // 6 BYTE
    case VK_FORMAT_R16G16B16_UNORM:
    case VK_FORMAT_R16G16B16_SNORM:
    case VK_FORMAT_R16G16B16_UINT:
    case VK_FORMAT_R16G16B16_SINT:
    case VK_FORMAT_R16G16B16_SFLOAT:
        return 6u;

    // 8 BYTE
    case VK_FORMAT_R16G16B16A16_UNORM:
    case VK_FORMAT_R16G16B16A16_SNORM:
    case VK_FORMAT_R16G16B16A16_UINT:
    case VK_FORMAT_R16G16B16A16_SINT:
    case VK_FORMAT_R16G16B16A16_SFLOAT:

    case VK_FORMAT_R32G32_UINT:
    case VK_FORMAT_R32G32_SINT:
    case VK_FORMAT_R32G32_SFLOAT:

    case VK_FORMAT_R64_UINT:
    case VK_FORMAT_R64_SINT:
    case VK_FORMAT_R64_SFLOAT:
        return 8u;

    // 12 BYTE
    case VK_FORMAT_R32G32B32_UINT:
    case VK_FORMAT_R32G32B32_SINT:
    case VK_FORMAT_R32G32B32_SFLOAT:
        return 12u;

    // 16 BYTE
    case VK_FORMAT_R32G32B32A32_UINT:
    case VK_FORMAT_R32G32B32A32_SINT:
    case VK_FORMAT_R32G32B32A32_SFLOAT:

    case VK_FORMAT_R64G64_UINT:
    case VK_FORMAT_R64G64_SINT:
    case VK_FORMAT_R64G64_SFLOAT:
        return 16u;

    // 24 BYTE
    case VK_FORMAT_R64G64B64_UINT:
    case VK_FORMAT_R64G64B64_SINT:
    case VK_FORMAT_R64G64B64_SFLOAT:
        return 24u;

    // 32 BYTE
    case VK_FORMAT_R64G64B64A64_UINT:
    case VK_FORMAT_R64G64B64A64_SINT:
    case VK_FORMAT_R64G64B64A64_SFLOAT:
        return 32u;

    // 64-bit 

    case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
    case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
    case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
    case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
    case VK_FORMAT_BC4_UNORM_BLOCK:
    case VK_FORMAT_BC4_SNORM_BLOCK:

    case VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK:
    case VK_FORMAT_EAC_R11_UNORM_BLOCK:
    case VK_FORMAT_EAC_R11_SNORM_BLOCK:
    case VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG:
    case VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG:
    case VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG:
    case VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG:
    case VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG:
    case VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG:
    case VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG:
    case VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG:
        return 8u;

    // 128-bit
    case VK_FORMAT_BC2_UNORM_BLOCK:
    case VK_FORMAT_BC2_SRGB_BLOCK:
    case VK_FORMAT_BC3_UNORM_BLOCK:
    case VK_FORMAT_BC3_SRGB_BLOCK:
    case VK_FORMAT_BC5_UNORM_BLOCK:
    case VK_FORMAT_BC5_SNORM_BLOCK:
    case VK_FORMAT_BC6H_UFLOAT_BLOCK:
    case VK_FORMAT_BC6H_SFLOAT_BLOCK:
    case VK_FORMAT_BC7_UNORM_BLOCK:
    case VK_FORMAT_BC7_SRGB_BLOCK:

    case VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK:
    case VK_FORMAT_EAC_R11G11_UNORM_BLOCK:
    case VK_FORMAT_EAC_R11G11_SNORM_BLOCK:
    case VK_FORMAT_ASTC_4x4_UNORM_BLOCK:
    case VK_FORMAT_ASTC_4x4_SRGB_BLOCK:
        return 16u;

    default:
        return 0u; // invalid
    }
}


void readImageData(PerThreadRenderContext rendererInfo, VmaAllocation alloc, VkImage image, void* data, VkExtent3D extent,
                   size_t level)
{
    auto vkCopyImageToMemoryEXT = (PFN_vkCopyImageToMemoryEXT)vkGetDeviceProcAddr(
        rendererInfo.device, "vkCopyImageToMemoryEXT");

    assert(vkCopyImageToMemoryEXT != VK_NULL_HANDLE);
    VkImageSubresourceLayers subresource = {VK_IMAGE_ASPECT_COLOR_BIT, static_cast<uint32_t>(level), 0, 1};
    VkImageToMemoryCopyEXT region = {
        VK_STRUCTURE_TYPE_IMAGE_TO_MEMORY_COPY_EXT, VK_NULL_HANDLE, data, 0, 0, subresource, {0, 0}, extent
    };
    VkCopyImageToMemoryInfoEXT info = {
        VK_STRUCTURE_TYPE_COPY_IMAGE_TO_MEMORY_INFO_EXT, VK_NULL_HANDLE, 0, image, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL, 1, &region
    };
    vkCopyImageToMemoryEXT(rendererInfo.device, &info);
}

std::basic_string<char> replaceExt(const char* target, const char* extension)
{
    std::basic_string<char> ktxPath;
    size_t ext = strcspn(target, ".");
    ktxPath.resize(ext + 4);

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)
#endif
    strncpy(ktxPath.data(), target, ext);
    strncpy(&ktxPath.data()[ext], extension, 4);
#ifdef _MSC_VER
#pragma warning(pop)
#endif

    return ktxPath;
}

constexpr uint32_t cubeMips = 6;

struct TextureLoadResultsQueue
{
    std::span<TextureMetaData> LoadResults;
    size_t processed;
};

TextureMetaData GetOrLoadTextureFromPath(PerThreadRenderContext rendererContext, const char* path, VkFormat format, VkSamplerAddressMode mode,
                                 TextureType textureType, bool use_mipmaps, bool compress)
{
    //TODO JS: Figure out cubes later -- when we add compression we should cache cubes too
    int maxmip = 0;
    if (textureType == CUBE)
    {
        maxmip = cubeMips;

        auto ktxResult = TextureCreation::createImageKTX(rendererContext, path, textureType, true, mode);
        VkFormat loadedFormat = ktxResult.dimensionsInfo.format;


        return ktxResult;
    }

    auto ktxPath = replaceExt(path, ".ktx");
    bool generateKTX = true;
    //Don't regenerate ktx if image modified time is older than last ktx 
    if (FileCaching::fileExists(ktxPath))
    {
        if (!FileCaching::assetOutOfDate(path))
        {
            generateKTX = false;
        }
    }
    // generateKTX = true;
    if (generateKTX)
    {
        auto bandp = rendererContext.textureCreationcommandPoolmanager->beginSingleTimeCommands(false);
        nonKTXTextureInfo staging = createtempTextureFromPath(rendererContext, bandp, path, format, use_mipmaps);
        rendererContext.textureCreationcommandPoolmanager->endSingleTimeCommands(bandp, true);
        cacheKTXFromTempTexture(rendererContext, staging, ktxPath.data(), format, textureType, use_mipmaps, compress);
        FileCaching::saveAssetChangedTime(path);
    }

    return TextureCreation::createImageKTX(rendererContext, ktxPath.data(), textureType, use_mipmaps, mode);
}


//TODO JS 0
// void TextureData::cleanup()
// {
//     vkDestroySampler(rendererContext.device, textureSampler, nullptr);
//     vkDestroyImageView(rendererContext.device, textureImageView, nullptr);
// 	   VulkanMemory::DestroyImage(rendererContext.allocator, textureImage, textureImageMemory);
// }
void TextureCreation::createDepthPyramidSampler(VkSampler* textureSampler, PerThreadRenderContext rendererContext, uint32_t maxMip)
{
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(rendererContext.physicalDevice, &properties);

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.minLod = 0; // Optional
    samplerInfo.maxLod = static_cast<float>(maxMip);

    //add a extension struct to enable Min mode
    VkSamplerReductionModeCreateInfoEXT createInfoReduction = {};

    createInfoReduction.sType = VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO_EXT;
    createInfoReduction.reductionMode = VK_SAMPLER_REDUCTION_MODE_MIN;
    samplerInfo.pNext = &createInfoReduction;
    VK_CHECK(vkCreateSampler(rendererContext.device, &samplerInfo, nullptr, textureSampler));
    rendererContext.threadDeletionQueue->push_backVk(deletionType::Sampler, uint64_t(*textureSampler));
}

void TextureCreation::createTextureSampler(VkSampler* textureSampler, PerThreadRenderContext rendererContext, VkSamplerAddressMode mode,
                          float bias, uint32_t maxMip, bool shadow)
{
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(rendererContext.physicalDevice, &properties);

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = mode;
    samplerInfo.addressModeV = mode;
    samplerInfo.addressModeW = mode;
    samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = shadow ? VK_TRUE : VK_FALSE;
    samplerInfo.compareOp = shadow ? VK_COMPARE_OP_LESS : VK_COMPARE_OP_ALWAYS;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.minLod = .0; // Optional
    samplerInfo.maxLod = static_cast<float>(maxMip);
    samplerInfo.mipLodBias = bias;


    VK_CHECK(vkCreateSampler(rendererContext.device, &samplerInfo, nullptr, textureSampler));
    rendererContext.threadDeletionQueue->push_backVk(deletionType::Sampler, uint64_t(*textureSampler));
}

//

void cacheKTXFromTempTexture(PerThreadRenderContext rendererContext, nonKTXTextureInfo tempTexture, const char* outpath,
                             VkFormat format, TextureType textureType, bool use_mipmaps, bool compress)
{

    ktx_size_t srcSize = 0;
    VkImage image = tempTexture.textureImage;
    ktxTexture2* texture; // For KTX2
    //ktxTexture1* texture;                 // For KTX
    ktxTextureCreateInfo createInfo;
    KTX_error_code result;
    //FILE* src;
    createInfo.vkFormat = format; // Ignored if creating a ktxTexture1.
    createInfo.baseWidth = static_cast<ktx_uint32_t>(tempTexture.width);
    createInfo.baseHeight = static_cast<ktx_uint32_t>(tempTexture.height);
    createInfo.baseDepth = 1; //TODO JS: ?????? I think only for 3d textures
    createInfo.numDimensions = 2;
    // Note: it is not necessary to provide a full mipmap pyramid.
    createInfo.numLevels = use_mipmaps ? static_cast<ktx_uint32_t>(tempTexture.mipCt) : 1;
    createInfo.numLayers = 1; //TOOD JS: Get this when we load cube 
    createInfo.numFaces = textureType == CUBE ? 6 : 1;
    createInfo.isArray = KTX_FALSE;
    createInfo.generateMipmaps = KTX_FALSE; // TODO JS: Make false and save mipmaps?


    result = ktxTexture2_Create(&createInfo,
                                KTX_TEXTURE_CREATE_NO_STORAGE,
                                &texture);

    uint32_t currentLevelWidth = static_cast<uint32_t>(tempTexture.width);
    uint32_t currentLevelheight = static_cast<uint32_t>(tempTexture.height);
    ktxTexture2_Create(&createInfo, KTX_TEXTURE_CREATE_ALLOC_STORAGE, &texture);
    for (uint32_t i = 0; i < createInfo.numLevels; i++)
    {
        int j = 0;
        int k = 0;
        int mipLevel = i;
        int layer = j;
        int faceSlice = k;

        std::vector<uint8_t> _imageData;
        const size_t imageByteSize = currentLevelWidth * currentLevelheight * static_cast<size_t>(
            getFormatSize(format));
        _imageData.resize(imageByteSize);

        readImageData(rendererContext, tempTexture.alloc, tempTexture.textureImage, _imageData.data(),
                      {currentLevelWidth, currentLevelheight, 1}, mipLevel);



        KTX_error_code r = ktxTexture_SetImageFromMemory(ktxTexture(texture), mipLevel, layer, faceSlice,
                                                         _imageData.data(), _imageData.size());
        currentLevelWidth /= 2;
        currentLevelheight /= 2;
    }

    if (compress)
    {
        printf("Compressing texture....\n");
        ktxTexture2_CompressBasis(texture, 1);
    }

    printf("Writing texture to file...\n");
    ktxTexture_WriteToNamedFile(ktxTexture(texture), outpath);
    ktxTexture_Destroy(ktxTexture(texture));

    //Destroy temporary texture
    //dupe
    // rendererContext.threadDeletionQueue->push_backVMA(deletionType::VmaImage, (uint64_t)tempTexture.textureImage, tempTexture.alloc);
}

VkImageView TextureCreation::createTextureImageView(PerThreadRenderContext rendererContext, TextureMetaData data, VkImageViewType type)
{
    VkImageView view = TextureUtilities::createImageView(objectCreationContextFromRendererContext(rendererContext),
                                                         data.textureImage.image, data.dimensionsInfo.format,
                                                         VK_IMAGE_ASPECT_COLOR_BIT, type, data.dimensionsInfo.layerCt,
                                                         0);


    return view;
}


static nonKTXTextureInfo createTextureImage(PerThreadRenderContext rendererContext, const unsigned char* pixels,
                                            uint64_t texWidth, uint64_t texHeight, VkFormat format, bool mips, CommandBufferPoolQueue workingTextureBuffer)
{
    VkDeviceSize imageSize = texWidth * texHeight * 4;
    assert(pixels);


    VkBuffer stagingBuffer;

    VmaAllocation bufferAlloc = {};

    VkImage _textureImage = {};
    VmaAllocation _textureImageAlloc;

    BufferUtilities::createStagingBuffer(imageSize,
                                         &bufferAlloc,
                                         stagingBuffer, rendererContext.device, rendererContext.allocator,
                                         "nonKTX texture");
    

    void* data;
    VulkanMemory::MapMemory(rendererContext.allocator, bufferAlloc, &data);
    memcpy(data, pixels, imageSize);
    VulkanMemory::UnmapMemory(rendererContext.allocator, bufferAlloc);


    uint32_t fullMipPyramid =
        mips ? static_cast<uint32_t>(std::floor(std::log2(glm::max(texWidth, texHeight)))) + 1 : 1;
    TextureUtilities::createImage(objectCreationContextFromRendererContext(rendererContext), texWidth, texHeight,
                                  format,
                                  VK_IMAGE_TILING_OPTIMAL,
                                  VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                  VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_HOST_TRANSFER_BIT_EXT,
                                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, _textureImage, _textureImageAlloc,
                                  fullMipPyramid);

    rendererContext.threadDeletionQueue->push_backVMA(deletionType::vmaBuffer, (uint64_t)stagingBuffer, bufferAlloc);

    //duplicate rendererContext.threadDeletionQueue->push_backVMA(deletionType::VmaImage, (uint64_t)_textureImage, _textureImageAlloc);

    TextureUtilities::transitionImageLayout(objectCreationContextFromRendererContext(rendererContext), _textureImage,
                                            format, VK_IMAGE_LAYOUT_UNDEFINED,
                                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, workingTextureBuffer->buffer,
                                            fullMipPyramid);

    TextureUtilities::copyBufferToImage(rendererContext.textureCreationcommandPoolmanager, stagingBuffer, _textureImage,
                                        static_cast<uint32_t>(texWidth),
                                        static_cast<uint32_t>(texHeight), workingTextureBuffer->buffer);

    //JS: Prepare image to read in shaders
    AllTextureAccessBarrier(workingTextureBuffer, _textureImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, VK_REMAINING_MIP_LEVELS);
    
    //Texture is done, generate mipmaps
    TextureUtilities::generateMipmaps(objectCreationContextFromRendererContext(rendererContext), _textureImage, format,
                                      static_cast<uint32_t>(texWidth),
                                      static_cast<uint32_t>(texHeight),
                                      fullMipPyramid, workingTextureBuffer); //TODO JS: centralize mip levels


    SetDebugObjectName(rendererContext.device, VK_OBJECT_TYPE_IMAGE, "temporary texture info image",
                       (uint64_t)_textureImage);
    SetDebugObjectName(rendererContext.device, VK_OBJECT_TYPE_BUFFER, "temporary texture info buffer",
                       (uint64_t)stagingBuffer);
    SetDebugObjectName(rendererContext.device, VK_OBJECT_TYPE_BUFFER, "temporary texture info buffer",
                       (uint64_t)stagingBuffer);

   

    // exit(1);

    return nonKTXTextureInfo(_textureImage, _textureImageAlloc, texWidth, texHeight, fullMipPyramid);
}

static nonKTXTextureInfo createtempTextureFromPath(PerThreadRenderContext rendererContext,CommandBufferPoolQueue buffer, const char* path, VkFormat format,
                                                   bool mips)
{
    int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load(path, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

    nonKTXTextureInfo tex = createTextureImage(rendererContext, pixels, texWidth, texHeight, format, mips,  buffer);
    stbi_image_free(pixels);
    return tex;
}

ktxVulkanDeviceInfo vdi = {};
TextureMetaData TextureCreation::createImageKTX(PerThreadRenderContext rendererContext, const char* path, TextureType type, bool mips, VkSamplerAddressMode mode)
{
   
    ktxVulkanTexture texture;

    ktxTexture2* kTexture;
    KTX_error_code ktxresult;
    GET_QUEUES()->graphicsQueueMutex.lock(); //todo js
    GET_QUEUES()->transferQueueMutex.lock(); //todo js
    ktxVulkanDeviceInfo_Construct(&vdi, rendererContext.physicalDevice, rendererContext.device,
                                 GET_QUEUES()->graphicsQueue,  rendererContext.textureCreationcommandPoolmanager->commandPool, nullptr);

    // Note: If I want to pass in an existing command buffer, I need to set it directly on vdi
    //This may be anti pattern, vdi may expect ownership of cbuffer(s)
    // vdi.cmdBuffer = workingTextureBuffer.buffer;
    ktxresult = ktxTexture2_CreateFromNamedFile(path, KTX_TEXTURE_CREATE_NO_STORAGE, &kTexture);

    if (KTX_SUCCESS != ktxresult)
    {
        std::cerr << "Creation of ktxTexture from \"" << path << "\" failed: " << ktxErrorString(ktxresult);
    }
    bool debugnomips = false;
    VkFormat outputFormat;

    //If it's a basis texture, we need to transcode (and also can't generate mipmaps)
    //TODO JS: paralellize, i think this is the main bottleneck for texture loading
    if (ktxTexture2_NeedsTranscoding(kTexture))
    {
        ktx_transcode_fmt_e transcodeFormat;

        transcodeFormat = KTX_TTF_BC1_OR_3;


        ktxTexture2_TranscodeBasis(kTexture, transcodeFormat, KTX_TF_HIGH_QUALITY);
        kTexture->generateMipmaps = false;
    }

    uint32_t fullMipPyramid = static_cast<uint32_t>(std::floor(
        std::log2(glm::max(kTexture->baseWidth, kTexture->baseHeight)))) + 1;
    uint32_t mipCount = kTexture->generateMipmaps ? fullMipPyramid : kTexture->numLevels;

    outputFormat = static_cast<VkFormat>(kTexture->vkFormat);

    ktxresult = ktxTexture2_VkUploadEx(kTexture, &vdi, &texture,
                                       VK_IMAGE_TILING_OPTIMAL,
                                       VK_IMAGE_USAGE_SAMPLED_BIT,
                                       VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);


    SetDebugObjectName(rendererContext.device, VK_OBJECT_TYPE_IMAGE, "KTX texture", (uint64_t)texture.image);


    ktxTexture_Destroy((ktxTexture*)kTexture);
    GET_QUEUES()->graphicsQueueMutex.unlock(); //todo js
    GET_QUEUES()->transferQueueMutex.unlock(); //todo js


#ifdef DEBUG
    if (KTX_SUCCESS != ktxresult)
    {
        std::cerr << "ktxTexture_VkUpload failed: " << ktxErrorString(ktxresult);
    }

#endif
    return {
        .textureImage = {texture.image, texture.deviceMemory},
        .dimensionsInfo = {
            .format = texture.imageFormat,
            .width = texture.width,
            .height = texture.height,
            .mipCt = type == CUBE ? cubeMips : texture.levelCount,
            .layerCt = texture.layerCount,
        },

        .ktxMemory = texture.deviceMemory,
        .mode = mode
    };
}

TextureCreation::TextureCreationStep1Result TextureCreation::CreateTextureFromArgs_Start(PerThreadRenderContext context, TextureCreation::TextureCreationInfoArgs a)
{
    TextureCreation::TextureCreationStep1Result r = {};
    r.mode = a.mode;
 
    switch (a.mode)
    {
    case TextureCreation::TextureCreationMode::FILE:
        {
            r.metaData = CreateTextureFromPath_Synchronously_Start(context, a.args.fileArgs);
            r.viewType = a.args.fileArgs.viewType;
            r.type = a.args.fileArgs.type;
            break;
        }
    case TextureCreation::TextureCreationMode::GLTFCREATE:
        r.metaData = CreateTextureNewGltfTexture_Start(context, a.args.gltfCreateArgs);
        break;
    case TextureCreation::TextureCreationMode::GLTFCACHED:
        r.metaData  = GetCachedTexture_Start(context, a.args.gltfCacheArgs);
        break;
assert("!error!");
break;
    }
    return r;
}
TextureData TextureCreation::CreateTextureSynchronously(PerThreadRenderContext context, TextureCreation::TextureCreationInfoArgs a)
{
    auto cbap = context.textureCreationcommandPoolmanager->beginSingleTimeCommands(false);
    auto s = CreateTextureFromArgs_Start( context, a);
    context.textureCreationcommandPoolmanager->endSingleTimeCommands(cbap, true);
    return CreateTextureFromArgsFinalize(context, s);
    
}
TextureData TextureCreation::CreateTextureFromArgsFinalize(PerThreadRenderContext outputTextureOwnerContext, TextureCreationStep1Result startResult)
{

    //These probably came from another thread, with its own deletion queue.
    //We can delete this when our rendercontext winds down;
    outputTextureOwnerContext.threadDeletionQueue->push_backVk(deletionType::Image, uint64_t(startResult.metaData.textureImage.image));
    outputTextureOwnerContext.threadDeletionQueue->push_backVk(deletionType::VkMemory, uint64_t(startResult.metaData.textureImage.memory));
    switch (startResult.mode)
    {
    case TextureCreationMode::FILE:
        return CreateTextureFromPath_Finalize(outputTextureOwnerContext, startResult.metaData, startResult.type, startResult.viewType);
    case TextureCreationMode::GLTFCREATE:
        return CreateTextureNewGltfTexture_Finalize(outputTextureOwnerContext, startResult.metaData);
    case TextureCreationMode::GLTFCACHED:
        return GetCachedTexture_Finalize(outputTextureOwnerContext, startResult.metaData);
    }
    assert("!Error");
    return {};
}

