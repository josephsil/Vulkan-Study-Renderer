
#include "vulkan-utilities.h"

#include <cassert>

#include "bufferCreation.h"
#include <General/Array.h>
#include "RendererContext.h"
#include "TextureData.h"
#include <General/MemoryArena.h>
#include "VulkanIncludes/Vulkan_Includes.h"



VkFormat Capabilities::findDepthFormat(VkPhysicalDevice physicalDevice)
{
    return findSupportedFormat(physicalDevice,
                               {VK_FORMAT_D16_UNORM},
                               VK_IMAGE_TILING_OPTIMAL,
                               VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT
    );
}


VkFormat Capabilities::findSupportedFormat(VkPhysicalDevice physicalDevice, const std::vector<VkFormat>& candidates,
                                           VkImageTiling tiling,
                                           VkFormatFeatureFlags features)
{
    for (VkFormat format : candidates)
    {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);

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

uint32_t Capabilities::findMemoryType(RendererContext rendererHandles, uint32_t typeFilter,
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


//TODO JS: Descriptor refactor Note 1-
    //TODO JS: I want to move descriptor allocation to the start of the frame
    //TODO JS: I want to allocate a varying descriptor set count in this call, and bump through them as I need them
    //TODO JS: I need to register how many I need for a frame... somewhere
    //TODO JS: OR: I can special case this for now, and expose something in pipeline group for how many sets there are
    //TODO JS: Span of sets?
    //TODO JS: And then in my draw command just consume them
    //TODO JS: Maybe that's better. 
void DescriptorSets::AllocateDescriptorSet(VkDevice device, VkDescriptorPool pool, VkDescriptorSetLayout* pdescriptorsetLayout, VkDescriptorSet* pset, uint32_t ct)
{
    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.pNext = nullptr;
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = pool;
    allocInfo.descriptorSetCount = ct;
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

