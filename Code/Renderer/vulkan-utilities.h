#pragma once
#include <span>
#include <vector>
#include <Renderer/VulkanIncludes/forward-declarations-renderer.h>
#include <Renderer/rendererGlobals.h>
#include <Renderer/RendererSharedTypes.h>
#include <Renderer/MainRenderer/PipelineManager/PipelineLayoutManager.h>


namespace MemoryArena
{
    struct memoryArena;
}

struct Vertex;
class AssetManager;
//Forward declaration
struct RendererContext;


namespace DescriptorSets
{
    //Passing around a vector of these to enforce binding for a pipeline
    VkDescriptorSetLayoutCreateInfo createInfoFromSpan(std::span<VkDescriptorSetLayoutBinding> bindings);
    VkDescriptorSetLayout createVkDescriptorSetLayout(RendererContext handles,
                                                      std::span<VkDescriptorSetLayoutBinding> layoutBindings,
                                                      const char* debugName);
    void AllocateDescriptorSet(VkDevice device, VkDescriptorPool pool, VkDescriptorSetLayout* pdescriptorsetLayout,
                               VkDescriptorSet* pset, size_t ct);
    void CreateDescriptorSetsForLayout(RendererContext handles, VkDescriptorPool pool, std::span<VkDescriptorSet> sets,
                                       VkDescriptorSetLayout layout, size_t descriptorCt, const char* debugName);
    void _updateDescriptorSet_NEW(RendererContext rendererHandles, VkDescriptorSet set,
                                  std::span<VkDescriptorSetLayoutBinding> setBindingInfo, std::span
                                  <descriptorUpdateData> descriptorUpdates);

    
    DescriptorDataForPipeline CreateDescriptorDataForPipeline(RendererContext ctx, VkDescriptorSetLayout layout,
                                                              bool isPerFrame,
                                                              std::span<VkDescriptorSetLayoutBinding> bindingLayout,
                                                              const char* setname, VkDescriptorPool pool,
                                                              int descriptorSetPoolSize = 1);
}

namespace Capabilities
{
    VkFormat findDepthFormat(VkPhysicalDevice physicalDevice);

    VkFormat findSupportedFormat(VkPhysicalDevice physicalDevice,
                                 const std::vector<VkFormat>& candidates,
                                 VkImageTiling tiling, VkFormatFeatureFlags features);
    uint32_t findMemoryType(RendererContext rendererHandles, uint32_t typeFilter, VkMemoryPropertyFlags properties);
}
