#define DEBUG 
#include "TextureData.h"
#include "ImageLibraryImplementations.h"
#include "vulkan-utilities.h"
#include "Vulkan_Includes.h"

#define KHRONOS_STATIC

#include <cassert>
#include <iostream>
#include <ktxvulkan.h>

#include "bufferCreation.h"
#include "CommandPoolManager.h"
#include "FileCaching.h"
#include "Memory.h"
#include "textureCreation.h"


int TEXTURE_INDEX;
#pragma region textureData

TextureData::TextureData(RendererHandles rendererHandles, const char* path, TextureType textureType)
{

    this->rendererHandles = rendererHandles;

    VkFormat inputFormat;
    VkSamplerAddressMode mode = VK_SAMPLER_ADDRESS_MODE_REPEAT;

    bool use_mipmaps = true;
    switch (textureType)
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
            mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            break;
        }
    }

    //TODO JS: branch better
	
    VkFormat loadedFormat = GetOrLoadTexture(path, inputFormat, textureType, use_mipmaps);
    createTextureImageView(loadedFormat, textureType == CUBE ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_2D);
    createTextureSampler(mode);
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


void readImageData(RendererHandles rendererInfo, VmaAllocation alloc, VkBuffer _buffer, void* data, size_t bytes,	size_t offset)
{
	void* tempData;
	 VulkanMemory::MapMemory(rendererInfo.allocator, alloc, &tempData);

	memcpy(data, tempData, bytes);
	 VulkanMemory::UnmapMemory(rendererInfo.allocator, alloc);
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

const uint32_t cubeMips = 6.0;


VkFormat TextureData::GetOrLoadTexture(const char* path, VkFormat format, TextureType textureType, bool use_mipmaps)
{
	//TODO JS: Figure out cubes later -- when we add compression we should cache cubes too
	if (textureType == CUBE)
	{
		maxmip = cubeMips;
		createImageKTX(path, textureType, true);
		return format; 
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
	 //generateKTX = true;
	if (generateKTX)
	{
		cacheKTXFromSTB(path, ktxPath.data(), format, textureType, use_mipmaps);
		FileCaching::saveAssetChangedTime(path);
	}
	
	return createImageKTX(ktxPath.data(), textureType, use_mipmaps);
}

TextureData::TextureData()
{
};

void TextureData::cleanup()
{
    vkDestroySampler(rendererHandles.device, textureSampler, nullptr);
    vkDestroyImageView(rendererHandles.device, textureImageView, nullptr);
	 VulkanMemory::DestroyImage(rendererHandles.allocator, textureImage, textureImageMemory);
}

void TextureData::createTextureSampler(VkSamplerAddressMode mode, float bias)
{
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(rendererHandles.physicalDevice, &properties);

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
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.minLod = .0; // Optional
    samplerInfo.maxLod = maxmip;
    samplerInfo.mipLodBias = bias;


    if (vkCreateSampler(rendererHandles.device, &samplerInfo, nullptr, &textureSampler) != VK_SUCCESS)
    {
       std::cerr << ("failed to create texture sampler!");
        exit(-1);
    }
}

void TextureData::cacheKTXFromSTB(const char* path, const char* outpath, VkFormat format, TextureType textureType, bool use_mipmaps)
{
	ktx_size_t srcSize;
    auto staging = createTextureImage(path, format, use_mipmaps);
        VkImage image = this->textureImage;
        ktxTexture2* texture;                   // For KTX2
        //ktxTexture1* texture;                 // For KTX
        ktxTextureCreateInfo createInfo;
        KTX_error_code result;
        FILE* src;
    createInfo.vkFormat = format;   // Ignored if creating a ktxTexture1.
    createInfo.baseWidth = staging.width;
    createInfo.baseHeight = staging.height;
    createInfo.baseDepth = 1; //TODO JS: ?????? I think only for 3d textures
    createInfo.numDimensions = 2;
    // Note: it is not necessary to provide a full mipmap pyramid.
    createInfo.numLevels = 1; //TOOD JS: Get this when we load cube  iData.mipLevels;
    createInfo.numLayers = 1; //TOOD JS: Get this when we load cube 
    createInfo.numFaces = textureType == CUBE ? 6 : 1; 
    createInfo.isArray = KTX_FALSE;
    createInfo.generateMipmaps = KTX_TRUE; // TODO JS: Make false and save mipmaps?

	
    result = ktxTexture2_Create(&createInfo,
                            KTX_TEXTURE_CREATE_NO_STORAGE,
                            &texture);
	
	int i = 0;

    std::vector<uint8_t> _imageData;

    const size_t imageByteSize = (size_t) staging.width * (size_t)staging.height * (size_t)getFormatSize(format);
    _imageData.resize(imageByteSize);
	readImageData(this->rendererHandles, staging.alloc, staging.buffer,_imageData.data(),imageByteSize, 0);

	ktxTexture2_Create(&createInfo, KTX_TEXTURE_CREATE_ALLOC_STORAGE, &texture);
    
    
	int j = 0;
	int k = 0;
    int mipLevel = i;
    int layer = j;
    int faceSlice = k;

	//TODO JS: For mipmaps, I need to get buffers pointing to each mip and set w miplevel
    KTX_error_code r = ktxTexture_SetImageFromMemory(ktxTexture(texture),mipLevel, layer,faceSlice,_imageData.data(),_imageData.size());

	VulkanMemory::DestroyBuffer(rendererHandles.allocator, staging.buffer, staging.alloc);

	//TODO JS: Can't generate mipmaps with uploadex() when using compressed formats
	//TODO JS: Need to generate mipmaps myself and save them to ktx 
	//ktxTexture2_CompressBasis(texture, 0);

	ktxTexture_WriteToNamedFile(ktxTexture(texture), outpath);
	ktxTexture_Destroy(ktxTexture(texture));

}

void TextureData::createTextureImageView(VkFormat format, VkImageViewType type)
{
    textureImageView = TextureUtilities::createImageView(rendererHandles.device, textureImage, format,
                                                         VK_IMAGE_ASPECT_COLOR_BIT, type, maxmip, layerct);
}


//TODO JS less side effects -- now that this is a interim step, it should be static
TextureData::temporaryTextureInfo TextureData::createTextureImage(const char* path, VkFormat format, bool mips)
{
    auto workingTextureBuffer = rendererHandles.commandPoolmanager->beginSingleTimeCommands(true);
    int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load(path, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    VkDeviceSize imageSize = texWidth * texHeight * 4;

    if (!pixels)
    {
         std::cerr << "failed to load texture image!";
        exit(-1);
    }


    VkBuffer stagingBuffer;

	VmaAllocation alloc = {};

    BufferUtilities::createStagingBuffer(rendererHandles, imageSize,
                                         &alloc,
                                         stagingBuffer);

    void* data;
 	VulkanMemory::MapMemory(rendererHandles.allocator,alloc, &data);
 	memcpy(data, pixels, imageSize);
 	VulkanMemory::UnmapMemory(rendererHandles.allocator,alloc);

    stbi_image_free(pixels);

    TextureUtilities::createImage(rendererHandles, texWidth, texHeight, format,
                                  VK_IMAGE_TILING_OPTIMAL,
                                  VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                  VK_IMAGE_USAGE_SAMPLED_BIT,
                                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, textureImage, textureImageMemory);

    TextureUtilities::transitionImageLayout(rendererHandles, textureImage, format, VK_IMAGE_LAYOUT_UNDEFINED,
                                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, workingTextureBuffer.buffer);

    TextureUtilities::copyBufferToImage(rendererHandles.commandPoolmanager, stagingBuffer, textureImage,
                                        static_cast<uint32_t>(texWidth),
                                        static_cast<uint32_t>(texHeight), workingTextureBuffer.buffer);
    //JS: Prepare image to read in shaders
    rendererHandles.commandPoolmanager->endSingleTimeCommands(workingTextureBuffer);
    return temporaryTextureInfo(stagingBuffer, alloc, texWidth, texHeight);

}



VkFormat TextureData::createImageKTX(const char* path, TextureType type, bool mips)
{
	ktxVulkanDeviceInfo vdi;
	ktxVulkanTexture texture;

	ktxTexture2* kTexture;
	KTX_error_code ktxresult;

	bufferAndPool workingTextureBuffer = rendererHandles.commandPoolmanager->beginSingleTimeCommands(false ); //TODO JS: Transfer pool doesnt work since ktx saving wor-- why?

	ktxVulkanDeviceInfo_Construct(&vdi, rendererHandles.physicalDevice, rendererHandles.device,
								  workingTextureBuffer.queue, workingTextureBuffer.pool, nullptr);

	ktxresult = ktxTexture2_CreateFromNamedFile(path, KTX_TEXTURE_CREATE_NO_STORAGE, &kTexture);

	if (KTX_SUCCESS != ktxresult)
	{
		std::cerr << "Creation of ktxTexture from \"" << path << "\" failed: " << ktxErrorString(ktxresult);
		exit(-1);
	}
	bool debugnomips = false;
	VkFormat outputFormat;

	//If it's a basis texture, we need to transcode (and also can't generate mipmaps) 
	if (ktxTexture2_NeedsTranscoding(kTexture))
	{
		ktxTexture2_TranscodeBasis(kTexture, KTX_TTF_BC3_RGBA, 0);
		kTexture->generateMipmaps = false;
	}
	
	uint32_t fullMipPyramid = static_cast<uint32_t>(std::floor(std::log2(max(kTexture->baseWidth, kTexture->baseHeight)))) + 1;
	uint32_t mipCount = kTexture->generateMipmaps ? fullMipPyramid : kTexture->numLevels;
	VkImage image;
	VkImageCreateInfo vkimageinfo = {};
	// VkImageCreateFlags       flags;
	vkimageinfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	vkimageinfo.imageType = kTexture->isCubemap ? VK_IMAGE_TYPE_3D : VK_IMAGE_TYPE_2D;
	vkimageinfo.format = (VkFormat)kTexture->vkFormat;
	vkimageinfo.extent = {kTexture->baseWidth, kTexture->baseHeight, kTexture->baseDepth};
	vkimageinfo.mipLevels = kTexture->isCubemap ? cubeMips : mipCount;
	vkimageinfo.arrayLayers = kTexture->numLayers;
	vkimageinfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	vkimageinfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
	vkimageinfo.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
	vkimageinfo.samples = VK_SAMPLE_COUNT_1_BIT;
	
    BufferUtilities::CreateImage(rendererHandles.allocator, &vkimageinfo, &texture.image, &textureImageMemory, &texture.deviceMemory);
	
	outputFormat = (VkFormat)kTexture->vkFormat;
	ktxresult = ktxTexture2_VkUploadEx(kTexture, &vdi, &texture,
									  VK_IMAGE_TILING_OPTIMAL,
									  VK_IMAGE_USAGE_SAMPLED_BIT,
									  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);


	ktxTexture_Destroy((ktxTexture*)kTexture);
	
	if (type != CUBE)
	{
		maxmip = texture.levelCount;
	}
	
	layerct = texture.layerCount;
	textureImage = texture.image;
	
	rendererHandles.commandPoolmanager->endSingleTimeCommands(workingTextureBuffer);

#ifdef DEBUG
	if (KTX_SUCCESS != ktxresult)
	{
		std::cerr << "ktxTexture_VkUpload failed: " << ktxErrorString(ktxresult);
		exit(-1);
	}

#endif
return outputFormat;
}



#pragma endregion
