#include <Renderer/vulkan-utilities.h>
#include <cassert>
#include <General/Array.h>
#include <Renderer/PerThreadRenderContext.h>
#include <General/MemoryArena.h>


VkFormat Capabilities::findDepthFormat(VkPhysicalDevice physicalDevice)
{
    return findSupportedFormat(physicalDevice,
                               {VK_FORMAT_D32_SFLOAT},
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

    assert(!"failed to find supported format!");
    exit(1);
}

uint32_t Capabilities::findMemoryType(PerThreadRenderContext rendererHandles, uint32_t typeFilter,
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
    exit(-1);
}


VkDescriptorSetLayoutCreateInfo DescriptorSets::createInfoFromSpan(std::span<VkDescriptorSetLayoutBinding> bindings)
{
    VkDescriptorSetLayoutCreateInfo _createInfo = {};
    _createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
     _createInfo.flags =   VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
    _createInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    _createInfo.pBindings = bindings.data();

    return _createInfo;
}

VkDescriptorSetLayout DescriptorSets::createVkDescriptorSetLayout(PerThreadRenderContext handles,
                                                                  std::span<VkDescriptorSetLayoutBinding>
                                                                  layoutBindings, const char* debugName)
{
    VkDescriptorSetLayoutCreateInfo layoutCreateInfo = createInfoFromSpan(layoutBindings);
    VkDescriptorBindingFlags binding_flags = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT;
    VkDescriptorSetLayoutBindingFlagsCreateInfoEXT extended_info = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT
    };
    extended_info.bindingCount = static_cast<uint32_t>(layoutBindings.size());
    std::span<VkDescriptorBindingFlagsEXT> extFlags = MemoryArena::AllocSpan<VkDescriptorBindingFlagsEXT>(
        handles.tempArena, layoutBindings.size());

    //enable partially bound bit for all samplers and images
    for (int i = 0; i < extFlags.size(); i++)
    {
        extFlags[i] = (layoutBindings[i].descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE || layoutBindings[i].
                          descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER)
                          ? binding_flags
                          : 0;
    }

    extended_info.pBindingFlags = extFlags.data();
    layoutCreateInfo.pNext = &extended_info;


    VkDescriptorSetLayout _layout = {};
    VK_CHECK(vkCreateDescriptorSetLayout(handles.device, &layoutCreateInfo, nullptr, &_layout));
    SetDebugObjectName(handles.device, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, debugName, (uint64_t)_layout);
    return _layout;
}

void DescriptorSets::AllocateDescriptorSet(VkDevice device, VkDescriptorPool pool,
                                           VkDescriptorSetLayout* pdescriptorsetLayout, VkDescriptorSet* pset,
                                           size_t ct)
{
    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.pNext = nullptr;
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = pool;
    allocInfo.descriptorSetCount = (uint32_t)ct;
    allocInfo.pSetLayouts = pdescriptorsetLayout;
    VkResult result = vkAllocateDescriptorSets(device, &allocInfo, pset);
    VK_CHECK(result);
}

void DescriptorSets::CreateDescriptorSetsForLayout(PerThreadRenderContext handles, VkDescriptorPool pool,
                                                   std::span<VkDescriptorSet> sets, VkDescriptorSetLayout layout,
                                                   size_t descriptorCt, const char* debugName)
{
    for (size_t i = 0; i < sets.size(); i++)
    {
        auto descriptorSetLayoutCopiesForAlloc = MemoryArena::AllocSpan<VkDescriptorSetLayout>(
            handles.tempArena, descriptorCt);

        for (size_t j = 0; j < descriptorCt; j++)
        {
            descriptorSetLayoutCopiesForAlloc[j] = layout;
        }

        AllocateDescriptorSet(handles.device, pool, descriptorSetLayoutCopiesForAlloc.data(), &sets[i], descriptorCt);
        SetDebugObjectName(handles.device, VK_OBJECT_TYPE_DESCRIPTOR_SET, debugName, uint64_t(sets[i]));
    }
}

void DescriptorSets::_updateDescriptorSet_NEW(PerThreadRenderContext rendererHandles, VkDescriptorSet set,
                                              std::span<VkDescriptorSetLayoutBinding> setBindingInfo,
                                              std::span<descriptorUpdateData> descriptorUpdates)
{
    Array<VkWriteDescriptorSet> writeDescriptorSets = MemoryArena::AllocSpan<VkWriteDescriptorSet>(
        rendererHandles.tempArena, setBindingInfo.size());
    for (int i = 0; i < descriptorUpdates.size(); i++)
    {
        descriptorUpdateData update = descriptorUpdates[i];

        VkDescriptorSetLayoutBinding bindingInfo = setBindingInfo[i];

        VkWriteDescriptorSet newSet = {};
        newSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        newSet.dstBinding = bindingInfo.binding;
        newSet.dstSet = set;
        newSet.descriptorCount = update.count;
        newSet.descriptorType = update.type;

        assert(update.type == bindingInfo.descriptorType);
        assert(update.count <= bindingInfo.descriptorCount);
        // assert(bindingIndex == bindingInfo.binding);

        if (update.type == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE || update.type == VK_DESCRIPTOR_TYPE_SAMPLER || update.type
            == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
        {
            newSet.pImageInfo = static_cast<VkDescriptorImageInfo*>(update.ptr);
        }
        else
        {
            newSet.pBufferInfo = static_cast<VkDescriptorBufferInfo*>(update.ptr);
        }

        writeDescriptorSets.push_back(newSet);
    }

    vkUpdateDescriptorSets(rendererHandles.device, static_cast<uint32_t>(writeDescriptorSets.size()),
                           writeDescriptorSets.getSpan().data(), 0, nullptr);
}

DescriptorDataForPipeline DescriptorSets::CreateDescriptorDataForPipeline(PerThreadRenderContext ctx,
    VkDescriptorSetLayout layout, bool isPerFrame, std::span<VkDescriptorSetLayoutBinding> bindingLayout,
    const char* setname, VkDescriptorPool pool, int descriptorSetPoolSize)
{
    //Construct DescriptorDataForPipeline
    int SetsForFrameCt = isPerFrame ? MAX_FRAMES_IN_FLIGHT : 1;
    PreAllocatedDescriptorSetPool* _DescriptorSets = MemoryArena::AllocSpan<PreAllocatedDescriptorSetPool>(
        ctx.arena, SetsForFrameCt).data();
    for (int i = 0; i < SetsForFrameCt; i++)
    {
        (_DescriptorSets[i]) = PreAllocatedDescriptorSetPool(ctx.arena, descriptorSetPoolSize);
    }
    auto bindlessLayoutBindings = MemoryArena::copySpan<VkDescriptorSetLayoutBinding>(ctx.arena, bindingLayout);

    DescriptorDataForPipeline outDescriptorData = {
        .isPerFrame = isPerFrame, .descriptorSetsCaches = _DescriptorSets, .layoutBindings = bindlessLayoutBindings
    };

    //initialize the VKdescriptorSets
    for (int i = 0; i < (isPerFrame ? MAX_FRAMES_IN_FLIGHT : 1); i++)
    {
        DescriptorSets::CreateDescriptorSetsForLayout(
            ctx, pool, outDescriptorData.descriptorSetsCaches[i].descriptorSets, layout, 1, setname);
    }

    return outDescriptorData;
}

VkDescriptorBufferInfo dataBuffer::getBufferInfo()
{
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = data; //TODO: For loop over frames in flight
    bufferInfo.offset = 0;
    bufferInfo.range = size;
    return bufferInfo;
}
