#define DEBUG 
#include "TextureData.h"
#include "vulkan-tutorial.h"
#include "stb_image.h"
#include "vulkan-utilities.h"

#define KHRONOS_STATIC
//TODO: for cubemap loadingc
#include <ktxvulkan.h>


int TEXTURE_INDEX;
#pragma region textureData

TextureData::TextureData(HelloTriangleApplication* app, const char* path, TextureType textureType)
{
    appref = app;

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
    vkDestroySampler(appref->device, textureSampler, nullptr);
    vkDestroyImageView(appref->device, textureImageView, nullptr);
    vkDestroyImage(appref->device, textureImage, nullptr);
    vkFreeMemory(appref->device, textureImageMemory, nullptr);
}

void TextureData::createTextureSampler(VkSamplerAddressMode mode, float bias)
{
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(appref->physicalDevice, &properties);

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


    if (vkCreateSampler(appref->device, &samplerInfo, nullptr, &textureSampler) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create texture sampler!");
    }
}

void TextureData::createTextureImageView(VkFormat format, VkImageViewType type)
{
    textureImageView = TextureUtilities::createImageView(appref->device, textureImage, format,
                                                         VK_IMAGE_ASPECT_COLOR_BIT, type, maxmip, layerct);
}


void TextureData::createTextureImage(const char* path, VkFormat format, bool mips)
{
    auto workingTextureBuffer = appref->commandPoolmanager.beginSingleTimeCommands(true);
    int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load(path, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    VkDeviceSize imageSize = texWidth * texHeight * 4;
    uint32_t mipLevels = !mips ? 1.0 : static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;
    maxmip = mipLevels;

    if (!pixels)
    {
        throw std::runtime_error("failed to load texture image!");
    }

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;

    BufferUtilities::createBuffer(appref, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                  stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(appref->device, stagingBufferMemory, 0, imageSize, 0, &data);
    memcpy(data, pixels, imageSize);
    vkUnmapMemory(appref->device, stagingBufferMemory);

    stbi_image_free(pixels);

    TextureUtilities::createImage(appref, texWidth, texHeight, format,
                                  VK_IMAGE_TILING_OPTIMAL,
                                  VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                  VK_IMAGE_USAGE_SAMPLED_BIT,
                                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, textureImage, textureImageMemory, mipLevels);

    TextureUtilities::transitionImageLayout(appref, textureImage, format, VK_IMAGE_LAYOUT_UNDEFINED,
                                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, workingTextureBuffer.buffer,
                                            mipLevels);

    TextureUtilities::copyBufferToImage(&appref->commandPoolmanager, stagingBuffer, textureImage,
                                        static_cast<uint32_t>(texWidth),
                                        static_cast<uint32_t>(texHeight), workingTextureBuffer.buffer);
    //JS: Prepare image to read in shaders
    appref->commandPoolmanager.endSingleTimeCommands(workingTextureBuffer);
    vkDestroyBuffer(appref->device, stagingBuffer, nullptr);
    vkFreeMemory(appref->device, stagingBufferMemory, nullptr);
    TextureUtilities::generateMipmaps(appref, textureImage, format, texWidth, texHeight, mipLevels);
}


void TextureData::createCubemapImageKTX(const char* path, VkFormat format)
{
    //TODO JS: Should i create these earlier?
    ktxVulkanDeviceInfo vdi;
    ktxVulkanTexture texture;

    ktxTexture* kTexture;
    KTX_error_code ktxresult;

    bufferAndPool workingTextureBuffer = appref->commandPoolmanager.beginSingleTimeCommands(true);

    ktxVulkanDeviceInfo_Construct(&vdi, appref->physicalDevice, appref->device,
                                  workingTextureBuffer.queue, workingTextureBuffer.pool, nullptr);

    ktxresult = ktxTexture_CreateFromNamedFile(path, KTX_TEXTURE_CREATE_NO_FLAGS, &kTexture);

    if (KTX_SUCCESS != ktxresult)
    {
        std::stringstream message;

        message << "Creation of ktxTexture from \"" << path << "\" failed: " << ktxErrorString(ktxresult);
        throw std::runtime_error(message.str());
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
        std::stringstream message;

        message << "ktxTexture_VkUpload failed: " << ktxErrorString(ktxresult);
        throw std::runtime_error(message.str());
    }

#endif

    //...
}


#pragma endregion
