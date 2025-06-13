#pragma once
#include <span>
#include <vector>
#include <Renderer/VulkanIncludes/forward-declarations-renderer.h>
#include <Renderer/rendererGlobals.h>
#include <Renderer/RendererSharedTypes.h>
#include <Renderer/MainRenderer/PipelineManager/PipelineLayoutManager.h>


namespace Allocator
{
    struct memoryArena;
}

struct Vertex;
class AssetManager;
//Forward declaration
struct PerThreadRenderContext;


namespace DescriptorSets
{
    //Passing around a vector of these to enforce binding for a pipeline
    VkDescriptorSetLayoutCreateInfo createInfoFromSpan(std::span<VkDescriptorSetLayoutBinding> bindings);
    VkDescriptorSetLayout createVkDescriptorSetLayout(PerThreadRenderContext handles,
                                                      std::span<VkDescriptorSetLayoutBinding> layoutBindings,
                                                      const char* debugName);
    void AllocateDescriptorSet(VkDevice device, VkDescriptorPool pool, VkDescriptorSetLayout* pdescriptorsetLayout,
                               VkDescriptorSet* pset, size_t ct);
    void CreateDescriptorSetsForLayout(PerThreadRenderContext handles, VkDescriptorPool pool, std::span<VkDescriptorSet> sets,
                                       VkDescriptorSetLayout layout, size_t descriptorCt, const char* debugName);
    void UpdateDescriptorSet(PerThreadRenderContext rendererHandles, VkDescriptorSet set,
                                  std::span<VkDescriptorSetLayoutBinding> setBindingInfo, std::span
                                  <DescriptorUpdateData> descriptorUpdates);

    
    DescriptorDataForPipeline CreateDescriptorDataForPipeline(PerThreadRenderContext ctx, VkDescriptorSetLayout layout,
                                                              bool isPerFrame,
                                                              std::span<VkDescriptorSetLayoutBinding> bindingLayout,
                                                              const char* setname, VkDescriptorPool pool,
                                                              int descriptorSetPoolSize = 1);
}

namespace Capabilities
{
    VkFormat FindDepthFormat(VkPhysicalDevice physicalDevice);

    VkFormat FindSupportedFormat(VkPhysicalDevice physicalDevice,
                                 const std::vector<VkFormat>& candidates,
                                 VkImageTiling tiling, VkFormatFeatureFlags features);
}
