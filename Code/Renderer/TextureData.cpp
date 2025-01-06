#define DEBUG 
#include "TextureData.h"
#include <ImageLibraryImplementations.h>
#include "vulkan-utilities.h"
#include "VulkanIncludes/Vulkan_Includes.h"

#include "VulkanIncludes/VulkanMemory.h"

#include <cassert>
#include <iostream>
#include <ktxvulkan.h>

#include "BufferAndPool.h"
#include "bufferCreation.h"
#include "CommandPoolManager.h"
#include <General/MemoryArena.h>
#include "textureCreation.h"
#include <General/FileCaching.h>

//Everywhere in the actual renderer we use ktx textures, but sometimes we load other textures from disk before caching them to ktx
//These structs are used for those temporary import textures. Need a better name / home
struct nonKTXTextureInfo
{
	VkImage textureImage;
	VmaAllocation alloc;
	uint64_t width;
	uint64_t  height;
	uint64_t  mipCt;
};

static nonKTXTextureInfo createtempTextureFromPath(RendererContext rendererContext, const char* path, VkFormat format, bool mips);
static nonKTXTextureInfo createTextureImage(RendererContext rendererContext, const unsigned char* pixels, uint64_t texWidth, uint64_t texHeight, VkFormat format, bool mips);
static void cacheKTXFromTempTexture(RendererContext rendererContext, nonKTXTextureInfo tempTexture, const char* outpath, VkFormat format, TextureType textureType, bool use_mipmaps, bool compress);
TextureMetaData GetOrLoadTexture(RendererContext rendererContext, const char* path, VkFormat format, TextureType textureType, bool use_mipmaps, bool compress);

TextureData createTexture(RendererContext rendererContext, const char* path, TextureType type, VkImageViewType viewType)
{

	VkFormat inputFormat;
	VkSamplerAddressMode mode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	bool uncompressed = false;

	bool use_mipmaps = true;
	switch (type)
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

	//TODO JS
	auto ktxResult = GetOrLoadTexture(rendererContext, path, inputFormat, type, use_mipmaps, !uncompressed);
	VkFormat loadedFormat = ktxResult.dimensionsInfo.format;
	auto layerct = ktxResult.dimensionsInfo.layerCt;
	auto textureImage = ktxResult.textureImage;
	
	if (viewType == -1)
	{
		viewType = type == CUBE ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_2D;
	}
	auto textureImageView = createTextureImageView(rendererContext, ktxResult, viewType);
	VkSampler textureSampler = {};
	createTextureSampler(&textureSampler, rendererContext, mode, 0, ktxResult.dimensionsInfo.mipCt);

	TextureData texture =
	{
		.vkImageInfo = {
			.sampler = textureSampler,
			.imageView = textureImageView,
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		},
		.metaData = {
			.textureImage = ktxResult.textureImage,
			.dimensionsInfo = ktxResult.dimensionsInfo,
			.ktxMemory = ktxResult.ktxMemory
		}

	};
	return texture;
}

//TODO JS: these constructors suck and are error prone, was just easier refactor
//FROM LOADED GLTF 
TextureData createTexture(RendererContext rendererContext, const char* OUTPUT_PATH, const char* textureName,  VkFormat format,  VkSamplerAddressMode samplerMode, unsigned char* pixels, uint64_t width, uint64_t height, int mipCt, bufferAndPool commandbuffer, bool compress)
{
	nonKTXTextureInfo staging = createTextureImage(rendererContext, pixels, width, height, format, true);
	//TODO JS: no gaurantee taking output path data works -- not null terminated rght?
	cacheKTXFromTempTexture(rendererContext, staging, OUTPUT_PATH, format, TextureType::DIFFUSE, mipCt != 0, compress);


	auto ktxResult = createImageKTX(rendererContext, OUTPUT_PATH, TextureType::DIFFUSE, true, true, &commandbuffer);
	VkFormat loadedFormat = ktxResult.dimensionsInfo.format;
	auto layerct = ktxResult.dimensionsInfo.layerCt;
	auto textureImage = ktxResult.textureImage;
	// textureImageMemory = ktxResult.ktxMemory;

	VkImageView textureImageView = createTextureImageView(rendererContext, ktxResult, VK_IMAGE_VIEW_TYPE_2D);
	assert(textureImageView != VK_NULL_HANDLE);
	VkSampler textureSampler = {};
	createTextureSampler(&textureSampler, rendererContext, samplerMode, 0, ktxResult.dimensionsInfo.mipCt);
	rendererContext = rendererContext;

	TextureData texture =
	{
		.vkImageInfo = {
			.sampler = textureSampler,
			.imageView = textureImageView,
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		},
		.metaData = {
			.textureImage = ktxResult.textureImage,
			.dimensionsInfo = ktxResult.dimensionsInfo,
			.ktxMemory = ktxResult.ktxMemory
		}

	};
return texture;
}

//FROM CACHED GLTF 
TextureData createTexture(RendererContext rendererContext, const char* OUTPUT_PATH, const char* textureName, VkFormat format, VkSamplerAddressMode samplerMode,
			uint64_t width, uint64_t height, int mipCt, bufferAndPool commandbuffer)
{
	auto ktxResult = createImageKTX(rendererContext, OUTPUT_PATH, TextureType::DIFFUSE, true, true, &commandbuffer);
	VkFormat loadedFormat = ktxResult.dimensionsInfo.format;
	
	auto textureImageView = createTextureImageView(rendererContext, ktxResult, VK_IMAGE_VIEW_TYPE_2D);
	assert(textureImageView != VK_NULL_HANDLE);

	VkSampler textureSampler = {};
	createTextureSampler(&textureSampler, rendererContext, samplerMode, 0, ktxResult.dimensionsInfo.mipCt);

	TextureData texture =
	{
		.vkImageInfo = {
			.sampler = textureSampler,
			.imageView = textureImageView,
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		},
		.metaData = {
			.textureImage = ktxResult.textureImage,
			.dimensionsInfo = ktxResult.dimensionsInfo,
			.ktxMemory = ktxResult.ktxMemory
		}

	};
return texture;
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


void readImageData(RendererContext rendererInfo, VmaAllocation alloc, VkImage image, void* data, VkExtent3D extent,size_t level)
{

	PFN_vkCopyImageToMemoryEXT vkCopyImageToMemoryEXT = (PFN_vkCopyImageToMemoryEXT)vkGetDeviceProcAddr(rendererInfo.device, "vkCopyImageToMemoryEXT");    

	assert(vkCopyImageToMemoryEXT != VK_NULL_HANDLE);
	VkImageSubresourceLayers subresource = {VK_IMAGE_ASPECT_COLOR_BIT, (uint32_t)level, 0, 1};
	VkImageToMemoryCopyEXT region = {VK_STRUCTURE_TYPE_IMAGE_TO_MEMORY_COPY_EXT, VK_NULL_HANDLE, data,0,0, subresource, {0,0}, extent};
	VkCopyImageToMemoryInfoEXT info = {VK_STRUCTURE_TYPE_COPY_IMAGE_TO_MEMORY_INFO_EXT, VK_NULL_HANDLE, 0, image, VK_IMAGE_LAYOUT_GENERAL, 1, &region};
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

const uint32_t cubeMips = 6;


TextureMetaData GetOrLoadTexture(RendererContext rendererContext, const char* path, VkFormat format, TextureType textureType, bool use_mipmaps, bool compress)
{
	//TODO JS: Figure out cubes later -- when we add compression we should cache cubes too
	int maxmip = 0;
	if (textureType == CUBE)
	{
		maxmip = cubeMips;
		
			auto ktxResult= createImageKTX(rendererContext, path, textureType, true);
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
		nonKTXTextureInfo staging = createtempTextureFromPath(rendererContext, path, format, use_mipmaps);
		cacheKTXFromTempTexture(rendererContext, staging, ktxPath.data(), format, textureType, use_mipmaps, compress);
		FileCaching::saveAssetChangedTime(path);
	}
	
	return createImageKTX(rendererContext, ktxPath.data(), textureType, use_mipmaps, false);
}


//TODO JS 0
// void TextureData::cleanup()
// {
//     vkDestroySampler(rendererContext.device, textureSampler, nullptr);
//     vkDestroyImageView(rendererContext.device, textureImageView, nullptr);
// 	   VulkanMemory::DestroyImage(rendererContext.allocator, textureImage, textureImageMemory);
// }

void createTextureSampler(VkSampler* textureSampler, RendererContext rendererContext, VkSamplerAddressMode mode, float bias, uint32_t maxMip, bool shadow)
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
    samplerInfo.maxLod = (float)maxMip;
    samplerInfo.mipLodBias = bias;


    VK_CHECK(vkCreateSampler(rendererContext.device, &samplerInfo, nullptr, textureSampler));
	rendererContext.rendererdeletionqueue->push_backVk(deletionType::Sampler, uint64_t(*textureSampler));
    
	
}
//

void cacheKTXFromTempTexture(RendererContext rendererContext, nonKTXTextureInfo tempTexture, const char* outpath, VkFormat format, TextureType textureType, bool use_mipmaps, bool compress)
{
	// assert(rendererContext.canWriteKTX);
	// if (!rendererContext.canWriteKTX)
	// {
	// 	exit (-1);
	// }
	ktx_size_t srcSize = 0;
	VkImage image = tempTexture.textureImage;
	ktxTexture2* texture;                   // For KTX2
	//ktxTexture1* texture;                 // For KTX
	ktxTextureCreateInfo createInfo;
	KTX_error_code result;
	//FILE* src;
    createInfo.vkFormat = format;   // Ignored if creating a ktxTexture1.
    createInfo.baseWidth = (ktx_uint32_t)tempTexture.width;
    createInfo.baseHeight = (ktx_uint32_t)tempTexture.height;
    createInfo.baseDepth = 1; //TODO JS: ?????? I think only for 3d textures
    createInfo.numDimensions = 2;
    // Note: it is not necessary to provide a full mipmap pyramid.
    createInfo.numLevels = use_mipmaps ? (ktx_uint32_t)tempTexture.mipCt : 1;
    createInfo.numLayers = 1; //TOOD JS: Get this when we load cube 
    createInfo.numFaces = textureType == CUBE ? 6 : 1; 
    createInfo.isArray = KTX_FALSE;
    createInfo.generateMipmaps = KTX_FALSE; // TODO JS: Make false and save mipmaps?

	
    result = ktxTexture2_Create(&createInfo,
                            KTX_TEXTURE_CREATE_NO_STORAGE,
                            &texture);
	
	uint32_t currentLevelWidth = (uint32_t) tempTexture.width;
	uint32_t currentLevelheight = (uint32_t) tempTexture.height;
	ktxTexture2_Create(&createInfo, KTX_TEXTURE_CREATE_ALLOC_STORAGE, &texture);
	for(uint32_t i = 0; i < createInfo.numLevels; i++)
	{
		int j = 0;
		int k = 0;
		int mipLevel = i;
		int layer = j;
		int faceSlice = k;
	
		std::vector<uint8_t> _imageData;
		const size_t imageByteSize = currentLevelWidth * currentLevelheight * (size_t)getFormatSize(format);
		_imageData.resize(imageByteSize);

		//the readimagedata call is extremely glacially slow -- need to take another approach
		readImageData(rendererContext, tempTexture.alloc, tempTexture.textureImage,_imageData.data(),{currentLevelWidth, currentLevelheight, 1}, mipLevel);

		
		KTX_error_code r = ktxTexture_SetImageFromMemory(ktxTexture(texture),mipLevel, layer,faceSlice,_imageData.data(),_imageData.size());
		currentLevelWidth /= 2;
		currentLevelheight /= 2;

	}

	if(compress)
	{
		printf("Compressing texture....\n" );
		ktxTexture2_CompressBasis(texture, 100);
	}

	ktxTexture_WriteToNamedFile(ktxTexture(texture), outpath);
	ktxTexture_Destroy(ktxTexture(texture));

	//Destroy temporary texture
	VulkanMemory::DestroyImage(rendererContext.allocator, tempTexture.textureImage, tempTexture.alloc); 
}

VkImageView createTextureImageView(RendererContext rendererContext,TextureMetaData data, VkImageViewType type)
{
   VkImageView view =  TextureUtilities::createImageView(objectCreationContextFromRendererContext(rendererContext), data.textureImage, data.dimensionsInfo.format,
                                                         VK_IMAGE_ASPECT_COLOR_BIT, type, data.dimensionsInfo.layerCt, 0);
	// rendererContext.rendererdeletionqueue->push_back(deleteableResource{deletionType::ImageView, view});
	

	return view;
}



static nonKTXTextureInfo createTextureImage(RendererContext rendererContext, const unsigned char* pixels, uint64_t texWidth, uint64_t texHeight, VkFormat format, bool mips)
{ 
	VkDeviceSize imageSize = texWidth * texHeight * 4;
	auto workingTextureBuffer = rendererContext.commandPoolmanager->beginSingleTimeCommands(true);
    if (!pixels)
    {
         std::cerr << "failed to load texture image!";
    exit(-1);
    }


    VkBuffer stagingBuffer;

	VmaAllocation bufferAlloc = {};

	VkImage _textureImage = {};
	VmaAllocation _textureImageAlloc;

    BufferUtilities::createStagingBuffer( imageSize,
                                         &bufferAlloc,
                                         stagingBuffer, rendererContext.device, rendererContext.allocator, "nonKTX texture");

    void* data;
 	VulkanMemory::MapMemory(rendererContext.allocator,bufferAlloc, &data);
 	memcpy(data, pixels, imageSize);
 	VulkanMemory::UnmapMemory(rendererContext.allocator,bufferAlloc);

  

	uint32_t fullMipPyramid =  mips ? static_cast<uint32_t>(std::floor(std::log2(glm::max(texWidth, texHeight)))) + 1: 1;
    TextureUtilities::createImage(objectCreationContextFromRendererContext(rendererContext), texWidth, texHeight, format,
                                  VK_IMAGE_TILING_OPTIMAL,
                                  VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                  VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_HOST_TRANSFER_BIT_EXT,
                                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, _textureImage, _textureImageAlloc, fullMipPyramid);

    TextureUtilities::transitionImageLayout(objectCreationContextFromRendererContext(rendererContext), _textureImage, format, VK_IMAGE_LAYOUT_UNDEFINED,
                                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, workingTextureBuffer.buffer, fullMipPyramid);

    TextureUtilities::copyBufferToImage(rendererContext.commandPoolmanager, stagingBuffer, _textureImage,
                                        static_cast<uint32_t>(texWidth),
                                        static_cast<uint32_t>(texHeight), workingTextureBuffer.buffer);
	
    //JS: Prepare image to read in shaders
    rendererContext.commandPoolmanager->endSingleTimeCommands(workingTextureBuffer);

	//Texture is done, generate mipmaps
	TextureUtilities::generateMipmaps(objectCreationContextFromRendererContext(rendererContext), _textureImage, format, static_cast<uint32_t>(texWidth),
									static_cast<uint32_t>(texHeight), fullMipPyramid);//TODO JS: centralize mip levels

	//Done building temporary texture and generating mipmaps -- transition back to host to return
	PFN_vkTransitionImageLayoutEXT vkTransitionImageLayoutEXT = (PFN_vkTransitionImageLayoutEXT)vkGetDeviceProcAddr(rendererContext.device, "vkTransitionImageLayoutEXT");
	assert(vkTransitionImageLayoutEXT != VK_NULL_HANDLE);
	VkImageSubresourceRange range = {VK_IMAGE_ASPECT_COLOR_BIT,0, fullMipPyramid, 0, 1};
	VkHostImageLayoutTransitionInfoEXT transtionInfo = {VK_STRUCTURE_TYPE_HOST_IMAGE_LAYOUT_TRANSITION_INFO_EXT, VK_NULL_HANDLE, _textureImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_IMAGE_LAYOUT_GENERAL, range};
	vkTransitionImageLayoutEXT(rendererContext.device,1, {&transtionInfo});


	setDebugObjectName(rendererContext.device, VK_OBJECT_TYPE_IMAGE, "temporary texture info image", (uint64_t)_textureImage);
	setDebugObjectName(rendererContext.device, VK_OBJECT_TYPE_BUFFER, "temporary texture info buffer", (uint64_t)stagingBuffer);
	setDebugObjectName(rendererContext.device, VK_OBJECT_TYPE_BUFFER, "temporary texture info buffer", (uint64_t)stagingBuffer);
	VulkanMemory::DestroyBuffer(rendererContext.allocator, stagingBuffer, bufferAlloc); // destroy temp buffer
	
	// exit(1);

    return nonKTXTextureInfo(_textureImage, _textureImageAlloc, texWidth, texHeight, fullMipPyramid);

}

static nonKTXTextureInfo createtempTextureFromPath(RendererContext rendererContext, const char* path, VkFormat format, bool mips)
{
	int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load(path, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

	nonKTXTextureInfo tex = createTextureImage(rendererContext, pixels, texWidth, texHeight, format, mips);
	stbi_image_free((void*)pixels);
	return tex;
}


TextureMetaData createImageKTX(RendererContext rendererContext, const char* path, TextureType type, bool mips,
                               bool useExistingBuffer, bufferAndPool* buffer)
{
	ktxVulkanDeviceInfo vdi;
	ktxVulkanTexture texture;

	ktxTexture2* kTexture;
	KTX_error_code ktxresult;

	bufferAndPool workingTextureBuffer;
	if (!useExistingBuffer)
	{
		workingTextureBuffer = rendererContext.commandPoolmanager->beginSingleTimeCommands(false ); //TODO JS: Transfer pool doesnt work since ktx saving wor-- why?
	}
	else
		workingTextureBuffer = *buffer;
	ktxVulkanDeviceInfo_Construct(&vdi, rendererContext.physicalDevice, rendererContext.device,
								  workingTextureBuffer.queue, workingTextureBuffer.pool, nullptr);

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
	//	switch (type)
		//{
		//default:
//			{
				transcodeFormat = KTX_TTF_BC1_OR_3;
	//		}
		//}

		
		ktxTexture2_TranscodeBasis(kTexture, transcodeFormat, KTX_TF_HIGH_QUALITY);
		kTexture->generateMipmaps = false;
	}
	
	uint32_t fullMipPyramid = static_cast<uint32_t>(std::floor(std::log2(glm::max(kTexture->baseWidth, kTexture->baseHeight)))) + 1;
	uint32_t mipCount = kTexture->generateMipmaps ? fullMipPyramid : kTexture->numLevels;
	
	outputFormat = (VkFormat)kTexture->vkFormat;
	
	ktxresult = ktxTexture2_VkUploadEx(kTexture, &vdi, &texture,
									  VK_IMAGE_TILING_OPTIMAL,
									  VK_IMAGE_USAGE_SAMPLED_BIT,
									  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	
	setDebugObjectName(rendererContext.device, VK_OBJECT_TYPE_IMAGE, "KTX texture", (uint64_t)texture.image);

	rendererContext.rendererdeletionqueue->push_backVk(deletionType::Image, uint64_t(texture.image));
	rendererContext.rendererdeletionqueue->push_backVk(deletionType::VkMemory, uint64_t(texture.deviceMemory));
	
	ktxTexture_Destroy((ktxTexture*)kTexture);

	

	
	if (!useExistingBuffer)
	{
		rendererContext.commandPoolmanager->endSingleTimeCommands(workingTextureBuffer);
	}

#ifdef DEBUG
	if (KTX_SUCCESS != ktxresult)
	{
		std::cerr << "ktxTexture_VkUpload failed: " << ktxErrorString(ktxresult);
	}

#endif
	return {
		.textureImage = texture.image,
		.dimensionsInfo = {
			.format = texture.imageFormat,
			.width = texture.width,
			.height = texture.height,
			.mipCt = type == CUBE ? cubeMips : texture.levelCount,
			.layerCt = texture.layerCount
		},
		
		.ktxMemory = texture.deviceMemory
	};
}
