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



static TextureCreation::stagingTextureData CreatetempTextureFromPath(PerThreadRenderContext rendererContext,CommandBufferPoolQueue buffer, const char* path, VkFormat format,
                                                   bool mips);
static TextureCreation::stagingTextureData CreateTextureImage(PerThreadRenderContext rendererContext, const unsigned char* pixels,
                                            uint64_t texWidth, uint64_t texHeight, VkFormat format, bool mips, CommandBufferPoolQueue workingTextureBuffer);
static void CacheImportedTextureToKTXFile(VkDevice device, TextureCreation::stagingTextureData tempTexture, const char* outpath,
                                    VkFormat format,  bool compress);
TextureCreation::TextureImportProcessTemporaryTexture GetOrLoadTextureFromPath(PerThreadRenderContext rendererContext, const char* path, VkFormat format, VkSamplerAddressMode mode,
                                 TextureType textureType, VkImageViewType imageViewType, bool useMipmaps, bool compress);

// TextureData TextureCreation::CreateTexture_part1(RendererContext rendererContext, const char* path, TextureType type, VkImageViewType viewType)
// {
// }
TextureCreation::LOAD_KTX_CACHED_args TextureCreation::CreateTexture_Cache_Temp_To_KTX_Step(
    PerThreadRenderContext rendererContext, TextureCreation::TextureImportProcessTemporaryTexture r)
{
    CacheImportedTextureToKTXFile(rendererContext.device,r.stagingTexture, r.outputPath,  r.format, 
       r.compress);

    // auto ktxResult = TextureCreation::CreateImageFromCachedKTX(rendererContext, r.outputPath, r.type, true, r.samplerMode);
        return   {
            .cachedKtxPath = r.outputPath,
            .samplerMode = r.samplerMode,
            .isCube =r.type == CUBE };
}

struct PerTypeImportSettings
{
    VkFormat formatOverride;
    VkSamplerAddressMode mode;
    bool uncompressed;
    bool use_mipmaps;
};
PerTypeImportSettings ImportSettingsForType(TextureType t)
{
    PerTypeImportSettings result =
        {
        .formatOverride = VK_FORMAT_R8G8B8A8_SRGB,
        .mode = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .uncompressed = false,
        .use_mipmaps = true
        };
    switch (t)
    {
    case DIFFUSE:
        {
            result.formatOverride = VK_FORMAT_R8G8B8A8_SRGB;
            break;
        }
    case SPECULAR:
        {
            result.formatOverride = VK_FORMAT_R8G8B8A8_UNORM;
            break;
        }
    case NORMAL:
        {
            result.formatOverride = VK_FORMAT_R8G8B8A8_UNORM;
            break;
        }
    case LINEAR_DATA:
        {
            result.formatOverride = VK_FORMAT_R8G8B8A8_UNORM;
            result.mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            result.use_mipmaps = false;
            break;
        }
    case CUBE:
        {
            result.formatOverride = VK_FORMAT_R8G8B8A8_UNORM;
            result.mode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            break;
        }
    case DATA_DONT_COMPRESS:
        {
            result.formatOverride = VK_FORMAT_R8G8B8A8_UNORM;
            result.mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            result.use_mipmaps = false;
            result.uncompressed = true;
        }
    }
    return result;
}
TextureCreation::TextureImportProcessTemporaryTexture CreateTextureFromPath(PerThreadRenderContext rendererContext, TextureCreation::IMPORT_FILE_args args)
{
    auto perTypeSettings = ImportSettingsForType(args.type);
    

    return GetOrLoadTextureFromPath(rendererContext, args.path, perTypeSettings.formatOverride, perTypeSettings.mode, args.type, args.viewType, perTypeSettings.use_mipmaps, !perTypeSettings.uncompressed);
}

TextureCreation::TextureImportRequest TextureCreation::MakeCreationArgsFromFilepathArgs(
    const char* path, ArenaAllocator arena, TextureType type, VkImageViewType viewType)
{
    if (viewType == -1)
    {
        viewType = type == CUBE ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_2D;
    }
    
    //Don't regenerate ktx if image modified time is older than last ktx
    std::span<char> resultPath = {};
    if (FileCaching::tryGetKTXCachedPath(arena, path, resultPath)) 
    {
        printf("Using cached texture at %s\n", resultPath.data()); //
        if (!FileCaching::assetOutOfDate(path))
        {
            auto settings = ImportSettingsForType(type);
            return MakeTextureCreationArgsFromCachedKTX(resultPath.data(),settings.mode, settings.uncompressed);
        }
    }
    
    TextureImportRequest args;
    args = TextureImportRequest{
        TextureImportMode::IMPORT_FILE
    };
    
    args.addtlData.fileArgs =
        IMPORT_FILE_args{
            .path = const_cast<char*>(path),
            .type = type,
            .viewType = viewType
        };
return args;
}

TextureCreation::TextureImportRequest TextureCreation::MakeTextureCreationArgsFromGLTFArgs(
    const char* OUTPUT_PATH, VkFormat format, VkSamplerAddressMode samplerMode, unsigned char* pixels, uint64_t width,
    uint64_t height, int mipCt, bool compress)
{
    TextureImportRequest args;
    args = TextureImportRequest{
        TextureImportMode::IMPORT_GLTF
    };
    args.addtlData.gltfCreateArgs =
        IMPORT_GLTF_args{
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

TextureCreation::TextureImportRequest TextureCreation::MakeTextureCreationArgsFromCachedKTX(
     const char* cachedFilePath, VkSamplerAddressMode samplerMode, bool isCube, bool nocompress)
{
    assert(!nocompress); //unused;
    TextureImportRequest args;
    args = TextureImportRequest{
        TextureImportMode::LOAD_KTX_CACHED
    };
    args.addtlData.gltfCacheArgs =
        LOAD_KTX_CACHED_args{
            .cachedKtxPath = const_cast<char*>(cachedFilePath),
            .samplerMode = samplerMode,
            .isCube =  isCube
        };
    return args;     
}


TextureCreation::TextureImportProcessTemporaryTexture CreateTextureFromGLTF(PerThreadRenderContext rendererContext,  TextureCreation::IMPORT_GLTF_args args)
{
    auto bandp = rendererContext.textureCreationcommandPoolmanager->beginSingleTimeCommands(false);
    TextureCreation::stagingTextureData staging = CreateTextureImage(rendererContext, args.pixels, args.width, args.height, args.format, true, bandp);

    //We need a fence between creating the texture and caching the ktx, because the ktx requires a read to host memory
    //I'd really rather go wide paralellizing the cache ktx, but I forgot how it worked when i started writing this. Someday I should break this up
    //I could also do the ktx thing on a background pool, and just return the texture I got from staging?
    rendererContext.textureCreationcommandPoolmanager->endSingleTimeCommands(bandp, true);

    TextureCreation::TextureImportProcessTemporaryTexture r =
        {
        .stagingTexture = staging,
        .outputPath = args.OUTPUT_PATH,
         .format = args.format,
        .compress = true,
        .samplerMode = args.samplerMode,
        .type =  DIFFUSE
        };

    return r;

}

TextureData CreateSamplerAndViewForLoadedTexture(PerThreadRenderContext rendererContext, TextureMetaData tData, VkImageViewType viewType)
{
    VkImageView textureImageView = TextureCreation::CreateTextureImageView(rendererContext,
        tData, viewType);
    assert(textureImageView != VK_NULL_HANDLE);
    VkSampler textureSampler = {};
    TextureCreation::CreateTextureSampler(&textureSampler,
        rendererContext,
        tData.mode, 0, tData.dimensionsInfo.mipCt);

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


void readImageData(VkDevice device, VkImage image, void* data, VkExtent3D extent,
                   uint32_t level)
{
    vkCopyImageToMemory(device, data, image, extent, level, 0, 1);
}


constexpr uint32_t cubeMips = 6;

struct TextureLoadResultsQueue
{
    std::span<TextureMetaData> LoadResults;
    size_t processed;
};

TextureCreation::TextureImportProcessTemporaryTexture GetOrLoadTextureFromPath(PerThreadRenderContext rendererContext, const char* path, VkFormat format, VkSamplerAddressMode mode,
                                 TextureType textureType, VkImageViewType imageViewType, bool useMipmaps, bool compress)
{

    auto bandp = rendererContext.textureCreationcommandPoolmanager->beginSingleTimeCommands(false);
    TextureCreation::stagingTextureData staging = CreatetempTextureFromPath(rendererContext, bandp, path, format, useMipmaps);
    rendererContext.textureCreationcommandPoolmanager->endSingleTimeCommands(bandp, true);
    std::span<char> ktxCachePath = {};
    FileCaching::tryGetKTXCachedPath(rendererContext.tempArena, path, ktxCachePath);
    TextureCreation::TextureImportProcessTemporaryTexture r =
       {
        .stagingTexture = staging,
        .outputPath = ktxCachePath.data(),
         .format = format,
        .compress = compress,
        .samplerMode = mode,
        .type = textureType,
        .viewType =  imageViewType
        };
    FileCaching::saveAssetChangedTime(path);
    return r;


}

void TextureCreation::CreateDepthPyramidSampler(VkSampler* textureSampler, VkSamplerReductionMode mode, PerThreadRenderContext rendererContext, uint32_t maxMip)
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
    samplerInfo.maxAnisotropy = 0;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_WHITE;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.minLod = 0; // Optional
    samplerInfo.maxLod = static_cast<float>(maxMip);

    //add a extension struct to enable Min mode
    VkSamplerReductionModeCreateInfoEXT createInfoReduction = {};

    createInfoReduction.sType = VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO_EXT;
    createInfoReduction.reductionMode = mode;
    samplerInfo.pNext = &createInfoReduction;
    VK_CHECK(vkCreateSampler(rendererContext.device, &samplerInfo, nullptr, textureSampler));
    rendererContext.threadDeletionQueue->push_backVk(deletionType::Sampler, uint64_t(*textureSampler));
}

void TextureCreation::CreateTextureSampler(VkSampler* textureSampler, PerThreadRenderContext rendererContext, VkSamplerAddressMode mode,
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

void CacheImportedTextureToKTXFile(VkDevice device, TextureCreation::stagingTextureData tempTexture, const char* outpath,
                             VkFormat format, bool compress)
{

    ktx_size_t srcSize = 0;
    VkImage image = tempTexture.textureImage;
    ktxTexture2* texture; // For KTX2
    ktxTextureCreateInfo createInfo;
    KTX_error_code result;
    createInfo.vkFormat = format; // Ignored if creating a ktxTexture1.
    createInfo.baseWidth = static_cast<ktx_uint32_t>(tempTexture.width);
    createInfo.baseHeight = static_cast<ktx_uint32_t>(tempTexture.height);
    createInfo.baseDepth = 1; //TODO JS: ?????? I think only for 3d textures
    createInfo.numDimensions = 2;
    createInfo.numLevels =static_cast<ktx_uint32_t>(tempTexture.mipCt);
    createInfo.numLayers = 1; //TOOD JS: Get this when we load cube 
    createInfo.numFaces = 1; //todo js: bug?
    createInfo.isArray = KTX_FALSE;
    createInfo.generateMipmaps = KTX_FALSE; 


    result = ktxTexture2_Create(&createInfo,
                                KTX_TEXTURE_CREATE_NO_STORAGE,
                                &texture);

    uint32_t currentLevelWidth = static_cast<uint32_t>(tempTexture.width);
    uint32_t currentLevelheight = static_cast<uint32_t>(tempTexture.height);
    ktxTexture2_Create(&createInfo, KTX_TEXTURE_CREATE_ALLOC_STORAGE, &texture);
    printf("Reading texture to cpu....\n");
    for (uint32_t mipLevel = 0; mipLevel < createInfo.numLevels; mipLevel++)
    {
        int layer = 0;
        int faceSlice = 0;

        std::vector<uint8_t> _imageData;
        const size_t imageByteSize = currentLevelWidth * currentLevelheight * static_cast<size_t>(
            getFormatSize(format));
        _imageData.resize(imageByteSize);

        readImageData( device, tempTexture.textureImage, _imageData.data(),
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
}

VkImageView TextureCreation::CreateTextureImageView(PerThreadRenderContext rendererContext, TextureMetaData data, VkImageViewType type)
{
    VkImageView view = TextureUtilities::createImageView(objectCreationContextFromRendererContext(rendererContext),
                                                         data.textureImage.image, data.dimensionsInfo.format,
                                                         VK_IMAGE_ASPECT_COLOR_BIT, type, data.dimensionsInfo.layerCt,
                                                         0);


    return view;
}


static TextureCreation::stagingTextureData CreateTextureImage(PerThreadRenderContext rendererContext, const unsigned char* pixels,
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

    return TextureCreation::stagingTextureData(_textureImage, _textureImageAlloc, texWidth, texHeight, fullMipPyramid);
}

static TextureCreation::stagingTextureData CreatetempTextureFromPath(PerThreadRenderContext rendererContext,CommandBufferPoolQueue buffer, const char* path, VkFormat format,
                                                   bool mips)
{
    int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load(path, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

    TextureCreation::stagingTextureData tex = CreateTextureImage(rendererContext, pixels, texWidth, texHeight, format, mips,  buffer);
    stbi_image_free(pixels);
    return tex;
}



TextureMetaData TextureCreation::CreateImageFromCachedKTX(PerThreadRenderContext rendererContext,const char* path, TextureType type, bool mips, VkSamplerAddressMode mode)
{
    superLuminalAdd("Cachedktxtexturecreation");
    ktxVulkanTexture texture;
    ktxVulkanDeviceInfo vdi = {};
    ktxTexture2* kTexture;
    KTX_error_code ktxresult;
    ktxVulkanDeviceInfo_Construct(&vdi, rendererContext.physicalDevice, rendererContext.device,
                                 GET_QUEUES()->graphicsQueue,  rendererContext.textureCreationcommandPoolmanager->commandPool, nullptr);

    ktxresult = ktxTexture2_CreateFromNamedFile(path, KTX_TEXTURE_CREATE_NO_STORAGE, &kTexture);

    if (KTX_SUCCESS != ktxresult)
    {
        std::cerr << "Creation of ktxTexture from \"" << path << "\" failed: " << ktxErrorString(ktxresult);
    }
    bool debugnomips = false;
    
    //If it's a basis texture, we need to transcode (and also can't generate mipmaps)
    if (ktxTexture2_NeedsTranscoding(kTexture))
    {
        ktx_transcode_fmt_e transcodeFormat;
        transcodeFormat = KTX_TTF_BC1_OR_3;
        ktxTexture2_TranscodeBasis(kTexture, transcodeFormat, KTX_TF_HIGH_QUALITY);
        kTexture->generateMipmaps = false;
    }

    uint32_t fullMipPyramid = static_cast<uint32_t>(std::floor(
        std::log2(glm::max(kTexture->baseWidth, kTexture->baseHeight)))) + 1;

    //Need to eventually get away from calling ktx vkupload -- because it manages it own command buffer/queue submission I have to mutex here :[ 
    GET_QUEUES()->graphicsQueueMutex.lock();
    ktxresult = ktxTexture2_VkUploadEx(kTexture, &vdi, &texture,
                                       VK_IMAGE_TILING_OPTIMAL,
                                       VK_IMAGE_USAGE_SAMPLED_BIT,
                                       VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    GET_QUEUES()->graphicsQueueMutex.unlock(); //todo js

    SetDebugObjectName(rendererContext.device, VK_OBJECT_TYPE_IMAGE, path, (uint64_t)texture.image);

    rendererContext.threadDeletionQueue->push_backVk(deletionType::KTXDestroyTexture, (uint64_t)kTexture);

#ifdef DEBUG
    if (KTX_SUCCESS != ktxresult)
    {
        std::cerr << "ktxTexture_VkUpload failed: " << ktxErrorString(ktxresult);
    }
    superLuminalEnd();

#endif
    return {
        .textureImage = {texture.image, texture.deviceMemory},
        .dimensionsInfo = {
            .format = texture.imageFormat,
            .width = texture.width,
            .height = texture.height,
            .mipCt = type == CUBE ? cubeMips : texture.levelCount,
            .layerCt = type == CUBE ? (uint32_t)6 : (uint32_t)1
        },

        .ktxMemory = texture.deviceMemory,
        .mode = mode
    };
}

TextureCreation::TextureImportProcessTemporaryTexture TextureCreation::GetTemporaryTextureDataForImportCache(
    PerThreadRenderContext context, TextureCreation::TextureImportRequest a)
{
    switch (a.mode)
    {
    case TextureCreation::TextureImportMode::IMPORT_FILE:
        {
           return CreateTextureFromPath(context, a.addtlData.fileArgs);
        }
    case TextureCreation::TextureImportMode::IMPORT_GLTF:
       return CreateTextureFromGLTF(context, a.addtlData.gltfCreateArgs);
   default:
        assert(!"Invalid texture import frontend type!");
        return {};
        break;
    }
}

TextureCreation::TextureImportResult TextureCreation::LoadAndUploadTextureFromImportCache(PerThreadRenderContext context, TextureCreation::LOAD_KTX_CACHED_args importArguments)
{
    TextureCreation::TextureImportResult r = {};
        r.metaData  = TextureCreation::CreateImageFromCachedKTX(context, importArguments.cachedKtxPath, importArguments.isCube ? CUBE : DIFFUSE, true, importArguments.samplerMode);
        r.viewType = importArguments.isCube ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_2D;
        return r;
}


TextureData TextureCreation::CreateTextureFromArgsFinalize(PerThreadRenderContext outputTextureOwnerContext, TextureImportResult startResult)
{
    //These probably came from another thread, with its own deletion queue.
    //We can delete these when our rendercontext winds down
    outputTextureOwnerContext.threadDeletionQueue->push_backVk(deletionType::Image, uint64_t(startResult.metaData.textureImage.image));
    outputTextureOwnerContext.threadDeletionQueue->push_backVk(deletionType::VkMemory, uint64_t(startResult.metaData.textureImage.memory));

    return CreateSamplerAndViewForLoadedTexture(outputTextureOwnerContext, startResult.metaData, startResult.viewType);
}

