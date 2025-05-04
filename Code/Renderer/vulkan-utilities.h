#pragma once
#include <span>
#include <unordered_map>
#include <vector>
#include "Renderer/VulkanIncludes/Vulkan_Includes.h"

#include "VulkanIncludes/forward-declarations-renderer.h"
#include "rendererGlobals.h"


namespace MemoryArena
{
    struct memoryArena;
}

struct Vertex;
class AssetManager;
//Forward declaration
struct RendererContext;
struct CommandPoolManager;



namespace DescriptorSets
{
    //Passing around a vector of these to enforce binding for a pipeline
    VkDescriptorSetLayoutCreateInfo createInfoFromSpan( std::span<VkDescriptorSetLayoutBinding> bindings);
    VkDescriptorSetLayout createVkDescriptorSetLayout(RendererContext handles,  std::span<VkDescriptorSetLayoutBinding> layoutBindings,  const char* debugName);
    void AllocateDescriptorSet(VkDevice device, VkDescriptorPool pool, VkDescriptorSetLayout* pdescriptorsetLayout, VkDescriptorSet* pset, uint32_t ct);
    void CreateDescriptorSetsForLayout(RendererContext handles, VkDescriptorPool pool, std::span<VkDescriptorSet> sets, VkDescriptorSetLayout layout,   uint32_t descriptorCt, const char* debugName);
    void _updateDescriptorSet_NEW(RendererContext rendererHandles, VkDescriptorSet set, std::span<VkDescriptorSetLayoutBinding> setBindingInfo, std::span
                                  <descriptorUpdateData> descriptorUpdates);


}

namespace Capabilities
{
    VkFormat findDepthFormat(VkPhysicalDevice physicalDevice);

    VkFormat findSupportedFormat(VkPhysicalDevice physicalDevice,
                                 const std::vector<VkFormat>& candidates,
                                 VkImageTiling tiling, VkFormatFeatureFlags features);    
    uint32_t findMemoryType(RendererContext rendererHandles, uint32_t typeFilter, VkMemoryPropertyFlags properties);
}


    