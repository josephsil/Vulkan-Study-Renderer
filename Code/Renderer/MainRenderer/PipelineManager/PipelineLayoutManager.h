#pragma once

#include <span>
#include <unordered_map>
#include "Internal/PipelineLayoutManagerTypes.h"

#include "General/Array.h"
#include "General/MemoryArena.h"
#include "Renderer/VulkanIncludes/Vulkan_Includes.h"

class PipelineLayoutGroup;
struct Vertex;


//The Pipeline Layout Manager provides access to PipelineLayoutGroups (See PipelineLayoutGroup.h)
//This handles all access to pipelines, from creation to rendering
//See VulkanRenderer.InitializePipelines()
//This was initially implemented very early on in my understanding of vulkan, and has been since contorted into a slightly better state
//May need an entire rethink.
struct  PipelineLayoutManager
{
    MemoryArena::Allocator arena{};
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

    FullShaderHandle CreatePipeline(PipelineLayoutHandle i, std::span<VkPipelineShaderStageCreateInfo> shaders,
                                    const char* name,
                                    GraphicsPipelineSettings settings);

    void cleanup();
};



