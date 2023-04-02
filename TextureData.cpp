#include "TextureData.h"
#include "vulkan-tutorial.h"

#include "stb_image.h"
int TEXTURE_INDEX;
#pragma region textureData
TextureData::TextureData(HelloTriangleApplication* app, const char* path)
{
    appref = app;
    createTextureImage(path);
    createTextureImageView();
    createTextureSampler();
    id = TEXTURE_INDEX++;
}

TextureData::TextureData()
{
}

void TextureData::cleanup()
{
    vkDestroySampler(appref->device, textureSampler, nullptr);
    vkDestroyImageView(appref->device, textureImageView, nullptr);
    vkDestroyImage(appref->device, textureImage, nullptr);
    vkFreeMemory(appref->device, textureImageMemory, nullptr);
}

void TextureData::createTextureSampler()
{
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(appref->physicalDevice, &properties);

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_TRUE;
    samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;

       

    if (vkCreateSampler(appref->device, &samplerInfo, nullptr, &textureSampler) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create texture sampler!");
    }
}

void TextureData::createTextureImageView()
{
    textureImageView = appref->createImageView(textureImage, VK_FORMAT_R8G8B8A8_SRGB);
}

void TextureData::createTextureImage(const char* path)
{
    auto workingTextureBuffer = appref->beginSingleTimeCommands();
    int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load(path, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    VkDeviceSize imageSize = texWidth * texHeight * 4;

    if (!pixels)
    {
        throw std::runtime_error("failed to load texture image!");
    }

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;

    appref->createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                         stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(appref->device, stagingBufferMemory, 0, imageSize, 0, &data);
    memcpy(data, pixels, static_cast<size_t>(imageSize));
    vkUnmapMemory(appref->device, stagingBufferMemory);

    stbi_image_free(pixels);

    appref->createImage(texWidth, texHeight, VK_FORMAT_R8G8B8A8_SRGB,
                        VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, textureImage, textureImageMemory);

    appref->transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED,
                                  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, workingTextureBuffer);
    appref->copyBufferToImage(stagingBuffer, textureImage, static_cast<uint32_t>(texWidth),
                              static_cast<uint32_t>(texHeight), workingTextureBuffer);

    appref->transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_SRGB,
                                  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, workingTextureBuffer);
    //JS: Prepare image to read in shaders

    appref->transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, workingTextureBuffer);

    appref->endSingleTimeCommands(workingTextureBuffer);
    vkDestroyBuffer(appref->device, stagingBuffer, nullptr);
    vkFreeMemory(appref->device, stagingBufferMemory, nullptr);
}

#pragma endregion