#define DEBUG 
#include "TextureData.h"
#include "ImageLibraryImplementations.h"
#include "vulkan-utilities.h"
#include "Vulkan_Includes.h"

#define KHRONOS_STATIC
//TODO: for cubemap loadingc
#include <cassert>
#include <iostream>
#include <ktxvulkan.h>

#include "CommandPoolManager.h"


int TEXTURE_INDEX;
#pragma region textureData

TextureData::TextureData(RendererHandles rendererHandles, const char* path, TextureType textureType)
{

    this->rendererHandles = rendererHandles;

    VkFormat format;
    VkSamplerAddressMode mode = VK_SAMPLER_ADDRESS_MODE_REPEAT;

    bool use_mipmaps = true;
    switch (textureType)
    {
    case DIFFUSE:
        {
            format = VK_FORMAT_R8G8B8A8_SRGB;
        }
    case SPECULAR:
        {
            format = VK_FORMAT_R8G8B8A8_UNORM;
        }
    case NORMAL:
        {
            format = VK_FORMAT_R8G8B8A8_UNORM;
        }
    case LINEAR_DATA:
        {
            format = VK_FORMAT_R8G8B8A8_UNORM;
            mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            use_mipmaps = false;
        }
    case CUBE:
        {
            format = VK_FORMAT_R8G8B8A8_UNORM;
            mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        }
    }

    //TODO JS: branch better
    textureType == CUBE ? createCubemapImageKTX(path, format) : createTextureImage(path, format, use_mipmaps);

    createTextureImageView(format, textureType == CUBE ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_2D);
    createTextureSampler(mode);
    id = TEXTURE_INDEX++;
}

TextureData::TextureData()
{
};

void TextureData::cleanup()
{
    vkDestroySampler(rendererHandles.device, textureSampler, nullptr);
    vkDestroyImageView(rendererHandles.device, textureImageView, nullptr);
    vkDestroyImage(rendererHandles.device, textureImage, nullptr);
    vkFreeMemory(rendererHandles.device, textureImageMemory, nullptr);
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

void TextureData::createTextureImageView(VkFormat format, VkImageViewType type)
{
    textureImageView = TextureUtilities::createImageView(rendererHandles.device, textureImage, format,
                                                         VK_IMAGE_ASPECT_COLOR_BIT, type, maxmip, layerct);
}


void TextureData::createTextureImage(const char* path, VkFormat format, bool mips)
{
    auto workingTextureBuffer = rendererHandles.commandPoolmanager->beginSingleTimeCommands(true);
    int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load(path, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    VkDeviceSize imageSize = texWidth * texHeight * 4;
    uint32_t mipLevels = !mips ? 1.0 : static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;
    maxmip = mipLevels;

    if (!pixels)
    {
         std::cerr << "failed to load texture image!";
        exit(-1);
    }

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;

    BufferUtilities::createBuffer(rendererHandles, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                  stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(rendererHandles.device, stagingBufferMemory, 0, imageSize, 0, &data);
    memcpy(data, pixels, imageSize);
    vkUnmapMemory(rendererHandles.device, stagingBufferMemory);

    stbi_image_free(pixels);

    TextureUtilities::createImage(rendererHandles, texWidth, texHeight, format,
                                  VK_IMAGE_TILING_OPTIMAL,
                                  VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                  VK_IMAGE_USAGE_SAMPLED_BIT,
                                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, textureImage, textureImageMemory, mipLevels);

    TextureUtilities::transitionImageLayout(rendererHandles, textureImage, format, VK_IMAGE_LAYOUT_UNDEFINED,
                                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, workingTextureBuffer.buffer,
                                            mipLevels);

    TextureUtilities::copyBufferToImage(rendererHandles.commandPoolmanager, stagingBuffer, textureImage,
                                        static_cast<uint32_t>(texWidth),
                                        static_cast<uint32_t>(texHeight), workingTextureBuffer.buffer);
    //JS: Prepare image to read in shaders
    rendererHandles.commandPoolmanager->endSingleTimeCommands(workingTextureBuffer);
    vkDestroyBuffer(rendererHandles.device, stagingBuffer, nullptr);
    vkFreeMemory(rendererHandles.device, stagingBufferMemory, nullptr);
    TextureUtilities::generateMipmaps(rendererHandles, textureImage, format, texWidth, texHeight, mipLevels);
}


void TextureData::createCubemapImageKTX(const char* path, VkFormat format)
{
    //TODO JS: Should i create these earlier?
    ktxVulkanDeviceInfo vdi;
    ktxVulkanTexture texture;

    ktxTexture* kTexture;
    KTX_error_code ktxresult;

    bufferAndPool workingTextureBuffer = rendererHandles.commandPoolmanager->beginSingleTimeCommands(true);

    ktxVulkanDeviceInfo_Construct(&vdi, rendererHandles.physicalDevice, rendererHandles.device,
                                  workingTextureBuffer.queue, workingTextureBuffer.pool, nullptr);

    ktxresult = ktxTexture_CreateFromNamedFile(path, KTX_TEXTURE_CREATE_NO_FLAGS, &kTexture);

    if (KTX_SUCCESS != ktxresult)
    {
         std::cerr << "Creation of ktxTexture from \"" << path << "\" failed: " << ktxErrorString(ktxresult);
        exit(-1);
    }


    //TODO JS: Should i use this helper elsewhere?
    ktxresult = ktxTexture_VkUploadEx(kTexture, &vdi, &texture,
                                      VK_IMAGE_TILING_OPTIMAL,
                                      VK_IMAGE_USAGE_SAMPLED_BIT,
                                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);


    ktxTexture_Destroy(kTexture);
    //TODO JS: destroy vdi?
    //TODO JS: is it right to throw away the ktxVulkanTexture here without a free or w/e?
    maxmip = 6.0;
    layerct = texture.layerCount;
    textureImage = texture.image;
    textureImageMemory = texture.deviceMemory;

#ifdef DEBUG
    if (KTX_SUCCESS != ktxresult)
    {
          std::cerr << "ktxTexture_VkUpload failed: " << ktxErrorString(ktxresult);
         exit(-1);
    }

#endif

    //...
}


#pragma endregion
