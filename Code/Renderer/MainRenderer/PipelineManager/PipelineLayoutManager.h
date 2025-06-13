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
struct PerThreadRenderContext;
struct DescriptorUpdateData;

typedef size_t PipelineLayoutHandle;
typedef size_t ShaderHandle;

struct FullShaderHandle
{
    PipelineLayoutHandle layout;
    ShaderHandle shader;
    bool operator==(FullShaderHandle const& rhs) const { return layout == rhs.layout && shader == rhs.shader; }
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

using descriptorUpdates = std::span<DescriptorUpdateData>;

struct DescriptorDataForPipeline
{
    bool isPerFrame;
    PreAllocatedDescriptorSetPool* descriptorSetsCaches;
    //Either an array of size MAX_FRAMES_IN_FLIGHT or 1 descriptorSetsForGroup depending on isPerFrame
    std::span<VkDescriptorSetLayoutBinding> layoutBindings;
};

struct GraphicsPipelineSettings
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

    PipelineLayoutHandle CreateNewGroup(PerThreadRenderContext handles, VkDescriptorPool pool,
                                        std::span<DescriptorDataForPipeline> descriptorInfo,
                                        std::span<VkDescriptorSetLayout> layouts, uint32_t pconstantsize, bool compute,
                                        const char* debugName);

    VkPipeline GetPipeline(PipelineLayoutHandle handle, ShaderHandle shaderIDX);
    VkPipeline GetPipeline(FullShaderHandle handle);
    DescriptorDataForPipeline* GetDescriptordata(PipelineLayoutHandle handle, ShaderHandle descriptorSetIndex);

    void BindRequiredSetsToCommandBuffer(PipelineLayoutHandle handle, VkCommandBuffer cmd, std::span<VkDescriptorSet> currentlyBoundSets,
                                                   uint32_t currentFrame, VkPipelineBindPoint
                                                   bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS);

    void ResetForFrame(size_t forFrame);
    VkPipelineLayout GetLayout(PipelineLayoutHandle handle);

    FullShaderHandle createPipeline(PipelineLayoutHandle i, std::span<VkPipelineShaderStageCreateInfo> shaders,
                                    const char* name,
                                    GraphicsPipelineSettings settings);

    size_t TODO_REMOVEgetPipelineCt(PipelineLayoutHandle handle);
    void cleanup();
};

//These map the shader ID in the scene/materials to shader IDs/pipeline groups for each major type
//Like, a material has an ID which indexes in here, and here's where we get the diffuse/shadow/depth shaders
struct BindlessObjectShaderGroup
{
    Array<FullShaderHandle> opaqueShaders;
    Array<FullShaderHandle> shadowShaders;
    Array<FullShaderHandle> depthOnlyShaders;
    BindlessObjectShaderGroup(MemoryArena::memoryArena*, size_t ct);
    BindlessObjectShaderGroup();
    void push_back(FullShaderHandle o, FullShaderHandle s);
};

