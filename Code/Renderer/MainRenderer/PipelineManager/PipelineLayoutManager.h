#pragma once


#pragma once

#include <span>
#include <unordered_map>
#include <vector>

#include "General/Array.h"
#include "General/MemoryArena.h"
#include "Renderer/VulkanIncludes/Vulkan_Includes.h"

class PipelineLayoutGroup;
struct Vertex;
//Forward declaration
struct RendererContext;
struct CommandPoolManager;
struct descriptorUpdateData;

typedef size_t PipelineLayoutHandle;
typedef size_t ShaderHandle;

struct fullPipelineHandle
{
    PipelineLayoutHandle x;
    ShaderHandle y;
};
//can have one or more descriptor sets to draw per frame. Most only have one
//Need multiple for cases where we need to "update" more than once per frame, for non-bindless style drawing
//TODO should remove this concept and dynamically grab them them from the pool as required

struct PreAllocatedDescriptorSetPool
{
    size_t activeDescriptorSet = SIZE_MAX;
    size_t descriptorsUsed = 0;
    std::span<VkDescriptorSet> descriptorSets;
    PreAllocatedDescriptorSetPool(MemoryArena::memoryArena* arena, size_t poolSize);
    void resetCacheForFrame();
    VkDescriptorSet getNextDescriptorSet();
    VkDescriptorSet peekNextDescriptorSet();
    void markCurrentDescriptorUsed();
};

using descriptorUpdates = std::span<descriptorUpdateData>;

struct DescriptorDataForPipeline
{
    bool isPerFrame;
    PreAllocatedDescriptorSetPool* descriptorSetsCaches;
    //Either an array of size MAX_FRAMES_IN_FLIGHT or 1 descriptorSetsForGroup depending on isPerFrame
    std::span<VkDescriptorSetLayoutBinding> layoutBindings;
};

struct graphicsPipelineSettings
{
    std::span<VkFormat> colorFormats;
    VkFormat depthFormat;
    VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT;
    VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkBool32 depthClampEnable = VK_FALSE;
    VkBool32 depthWriteEnable = VK_TRUE;
    VkBool32 depthBias = VK_FALSE;
    bool dynamicBias = false;
};



struct  PipelineLayoutManager
{
    MemoryArena::memoryArena arena{};
    Array<PipelineLayoutGroup> pipelineLayoutGroups;
    PipelineLayoutManager();

    PipelineLayoutHandle CreateNewGroup(RendererContext handles, VkDescriptorPool pool,
                                        std::span<DescriptorDataForPipeline> descriptorInfo,
                                        std::span<VkDescriptorSetLayout> layouts, uint32_t pconstantsize, bool compute,
                                        const char* debugName);

    VkPipeline GetPipeline(PipelineLayoutHandle handle, ShaderHandle shaderIDX);
    DescriptorDataForPipeline* GetDescriptordata(PipelineLayoutHandle handle, ShaderHandle descriptorSetIndex);

    void BindRequiredSetsToCommandBuffer(PipelineLayoutHandle handle, VkCommandBuffer cmd, std::span<VkDescriptorSet> currentlyBoundSets,
                                                   uint32_t currentFrame, VkPipelineBindPoint
                                                   bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS);

    void ResetForFrame(size_t forFrame);
    VkPipelineLayout GetLayout(PipelineLayoutHandle handle);
    
    void createPipeline(PipelineLayoutHandle i, std::span<VkPipelineShaderStageCreateInfo> shaders, const char* name,
                    graphicsPipelineSettings settings);

    size_t TODO_REMOVEgetPipelineCt(PipelineLayoutHandle handle);
    void cleanup();
};