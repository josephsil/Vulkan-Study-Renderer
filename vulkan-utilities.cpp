
#include "vulkan-utilities.h"

#include "AppStruct.h"
#include "CommandPoolManager.h"
#include "TextureData.h"
#include "Vulkan_Includes.h"
#include "SceneObjectData.h"






VkFormat Capabilities::findDepthFormat(RendererHandles rendererHandles)
{
    return findSupportedFormat(rendererHandles,
                               {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
                               VK_IMAGE_TILING_OPTIMAL,
                               VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
    );
}


VkFormat Capabilities::findSupportedFormat(RendererHandles rendererHandles, const std::vector<VkFormat>& candidates,
                                           VkImageTiling tiling,
                                           VkFormatFeatureFlags features)
{
    for (VkFormat format : candidates)
    {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(rendererHandles.physicalDevice, format, &props);

        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features)
        {
            return format;
        }
        if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features)
        {
            return format;
        }
    }

    printf("failed to find supported format!");
    exit(1);
}

uint32_t Capabilities::findMemoryType(RendererHandles rendererHandles, uint32_t typeFilter,
                                      VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(rendererHandles.physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
    {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
        {
            return i;
        }
    }

    printf("failed to find suitable memory type!");
    exit (-1);
}


//This probably doesnt need to exisdt
std::pair<std::vector<VkDescriptorImageInfo>, std::vector<VkDescriptorImageInfo>>
DescriptorSets::ImageInfoFromImageDataVec(std::vector<TextureData> textures)
{
    std::vector<VkDescriptorImageInfo> imageinfos(textures.size());
    std::vector<VkDescriptorImageInfo> samplerinfos(textures.size());
    for (int i = 0; i < textures.size(); i++)
    {
        imageinfos[i] = VkDescriptorImageInfo{
            .imageView = textures[i].textureImageView, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };
        samplerinfos[i] = VkDescriptorImageInfo{
            .sampler = textures[i].textureSampler, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };
    }

    return std::make_pair(imageinfos, samplerinfos);
}

void DescriptorSets::AllocateDescriptorSet(VkDevice device, VkDescriptorPool pool, VkDescriptorSetLayout* pdescriptorsetLayout, VkDescriptorSet* pset)
{
    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.pNext = nullptr;
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = pool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = pdescriptorsetLayout;
    VkResult result = vkAllocateDescriptorSets(device, &allocInfo, pset);
    VK_CHECK(result);
  
}


VkDescriptorBufferInfo dataBuffer::getBufferInfo()
{
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = data; //TODO: For loop over frames in flight
    bufferInfo.offset = 0;
    bufferInfo.range = size;
    return bufferInfo;
}
