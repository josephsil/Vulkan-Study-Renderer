
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
                               {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
                               VK_IMAGE_TILING_OPTIMAL,
                               VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
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

void dataBuffer::updateMappedMemory(void* data, size_t size)
{
    assert(size == this->size);
    memcpy(this->mapped, data, size);
}

void dataBuffer::allocateVulkanMemory(RendererContext h, VmaAllocation* allocation, VkBufferUsageFlags usage)
{
   mapped = BufferUtilities::createDynamicBuffer(
      h,  size, usage,
     allocation,
    data);

    h.rendererdeletionqueue->push_backVMA(deletionType::vmaBuffer, uint64_t(data), *allocation);
}

