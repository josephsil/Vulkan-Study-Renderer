
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


VkDescriptorSetLayoutCreateInfo  DescriptorSets::createInfoFromSpan( std::span<VkDescriptorSetLayoutBinding> bindings)
{
    VkDescriptorSetLayoutCreateInfo _createInfo = {};
    _createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    // _createInfo.flags =   VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
    _createInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    _createInfo.pBindings = bindings.data();

    return _createInfo;
}

VkDescriptorSetLayout  DescriptorSets::createVkDescriptorSetLayout(RendererContext handles,  std::span<VkDescriptorSetLayoutBinding> layoutBindings,  const char* debugName)
{
    VkDescriptorSetLayoutCreateInfo layoutCreateInfo = createInfoFromSpan(layoutBindings);
    VkDescriptorBindingFlags binding_flags = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT; 
    VkDescriptorSetLayoutBindingFlagsCreateInfoEXT extended_info = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT};
    extended_info.bindingCount = (uint32_t)layoutBindings.size(); 
    std::span<VkDescriptorBindingFlagsEXT> extFlags = MemoryArena::AllocSpan<VkDescriptorBindingFlagsEXT>(handles.tempArena, layoutBindings.size());

    //enable partially bound bit for all samplers and images
    for (int i = 0; i < extFlags.size(); i++)
    {
        extFlags[i] = (layoutBindings[i].descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE || layoutBindings[i].descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER) ? binding_flags : 0;
    }
   
    extended_info.pBindingFlags = extFlags.data();
    layoutCreateInfo.pNext = &extended_info;



    VkDescriptorSetLayout _layout = {};
    VK_CHECK(vkCreateDescriptorSetLayout(handles.device, &layoutCreateInfo, nullptr, &_layout));
    setDebugObjectName(handles.device, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, debugName,(uint64_t)_layout );
    return _layout;
}

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

void DescriptorSets::CreateDescriptorSetsForLayout(RendererContext handles, VkDescriptorPool pool, std::span<VkDescriptorSet> sets, VkDescriptorSetLayout layout,   uint32_t descriptorCt, const char* debugName)
{

    for(int i = 0; i < sets.size(); i++)
    {
        auto descriptorSetLayoutCopiesForAlloc = MemoryArena::AllocSpan<VkDescriptorSetLayout>(handles.tempArena,descriptorCt);

        for(int j = 0; j < descriptorCt; j++)
        {
            descriptorSetLayoutCopiesForAlloc[j] = layout;
        }

        DescriptorSets::AllocateDescriptorSet(handles.device, pool,  descriptorSetLayoutCopiesForAlloc.data(), &sets[i], descriptorCt);
        setDebugObjectName(handles.device, VK_OBJECT_TYPE_DESCRIPTOR_SET,debugName, uint64_t(sets[i]) );
    }

}

VkDescriptorBufferInfo dataBuffer::getBufferInfo()
{
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = data; //TODO: For loop over frames in flight
    bufferInfo.offset = 0;
    bufferInfo.range = size;
    return bufferInfo;
}

