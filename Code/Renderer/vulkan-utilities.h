#pragma once
#include <span>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan_core.h>

#include "VulkanIncludes/forward-declarations-renderer.h"
#include "rendererGlobals.h"


namespace MemoryArena
{
    struct memoryArena;
}

struct Vertex;
class Scene;
//Forward declaration
struct RendererHandles;
struct CommandPoolManager;
struct TextureData;



namespace DescriptorSets
{
    std::span<VkDescriptorImageInfo> ImageInfoFromImageDataVec(MemoryArena::memoryArena* arena, std::vector<TextureData> textures);
    
    //Passing around a vector of these to enforce binding for a pipeline
    
    void AllocateDescriptorSet(VkDevice device, VkDescriptorPool pool, VkDescriptorSetLayout* pdescriptorsetLayout, VkDescriptorSet* pset);

   

}

namespace Capabilities
{
    VkFormat findDepthFormat(RendererHandles rendererHandles);

    VkFormat findSupportedFormat(RendererHandles rendererHandles, const std::vector<VkFormat>& candidates,
                                 VkImageTiling tiling,
                                 VkFormatFeatureFlags features);    
    uint32_t findMemoryType(RendererHandles rendererHandles, uint32_t typeFilter, VkMemoryPropertyFlags properties);
}


    