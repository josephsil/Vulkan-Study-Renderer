#include <cassert>

#include "../../rendererGlobals.h"
#include "../../vulkan-utilities.h"
#include "../../VulkanIncludes/Vulkan_Includes.h"

//TODO JS: break this dependency
#include "PipelineLayoutManager.h"

#include <span>
#include "Internal/PipelineLayoutGroup.h"
#include <General/MemoryArena.h>
#include <Renderer/RendererContext.h>

PreAllocatedDescriptorSetPool::PreAllocatedDescriptorSetPool(MemoryArena::memoryArena* arena, size_t poolSize)
{
    descriptorSets = MemoryArena::AllocSpan<VkDescriptorSet>(arena, poolSize);
}

PipelineLayoutManager::PipelineLayoutManager()
{
    MemoryArena::initialize(&arena, 4000);
    pipelineLayoutGroups = MemoryArena::AllocSpan<PipelineLayoutGroup>(&arena, 10);
}

PipelineLayoutHandle PipelineLayoutManager::CreateNewGroup(RendererContext handles, VkDescriptorPool pool,
                                                           std::span<DescriptorDataForPipeline> descriptorInfo, std::span<VkDescriptorSetLayout> layouts,
                                                           uint32_t pconstantsize, bool compute, const char* debugName)
{
    pipelineLayoutGroups.emplace_back(handles, pool, descriptorInfo, layouts, pconstantsize, compute, debugName);
    return pipelineLayoutGroups.ct -1;
}

VkPipeline PipelineLayoutManager::GetPipeline(PipelineLayoutHandle pipelineIdx, ShaderHandle shaderIDX)
{
    return  pipelineLayoutGroups[pipelineIdx].getPipeline(shaderIDX);
}
VkPipeline PipelineLayoutManager::GetPipeline(FullShaderHandle handle)
{
    return  pipelineLayoutGroups[handle.layout].getPipeline(handle.shader);
}

DescriptorDataForPipeline* PipelineLayoutManager::GetDescriptordata(PipelineLayoutHandle pipelineIdx,
    size_t descriptorSetIndex)
{
    return &pipelineLayoutGroups[pipelineIdx].layoutData.descriptorConfiguration[descriptorSetIndex];
}

void PipelineLayoutManager::BindRequiredSetsToCommandBuffer(PipelineLayoutHandle handle, VkCommandBuffer cmd,
    std::span<VkDescriptorSet> currentlyBoundSets, uint32_t currentFrame, VkPipelineBindPoint bindPoint)
{
    pipelineLayoutGroups[handle].BindRequiredDescriptorSetsToCommandBuffer(cmd, currentlyBoundSets, currentFrame, bindPoint);
}

void PipelineLayoutManager::ResetForFrame(size_t forFrame)
{
        //Reset the descriptor set cache for the frame
        for(int i = 0; i < pipelineLayoutGroups.size(); i++)
        {
            auto descriptorData = pipelineLayoutGroups[i].layoutData.descriptorConfiguration;
            for (auto& element : descriptorData)
            {
                if (element.isPerFrame)
                {
                        (element.descriptorSetsCaches[forFrame]).resetCacheForFrame();
                }
            }
        }
}

VkPipelineLayout PipelineLayoutManager::GetLayout(PipelineLayoutHandle handle)
{
    return pipelineLayoutGroups[handle].layoutData.layout;
}

FullShaderHandle PipelineLayoutManager::createPipeline(PipelineLayoutHandle i,
                                                       std::span<VkPipelineShaderStageCreateInfo> shaders,
                                                       const char* name,
                                                       graphicsPipelineSettings settings)
{
    pipelineLayoutGroups[i].createPipeline(shaders, name, settings);
    return {i, pipelineLayoutGroups[i].getPipelineCt() -1};
}

size_t PipelineLayoutManager::TODO_REMOVEgetPipelineCt(PipelineLayoutHandle handle)
{
    return pipelineLayoutGroups[handle].getPipelineCt();
}

void PipelineLayoutManager::cleanup()
{
    for(int i = 0; i < pipelineLayoutGroups.size(); i ++)
    {
        pipelineLayoutGroups[i].cleanup();
    }
}

BindlessObjectShaderGroup::BindlessObjectShaderGroup(MemoryArena::memoryArena* a, size_t ct)
{
    auto backing = MemoryArena::AllocSpan<FullShaderHandle>(a, ct * 3);
    opaqueShaders = Array(backing.subspan(0, ct));
    shadowShaders = Array(backing.subspan(ct, ct));
    depthOnlyShaders = Array(backing.subspan(ct*2, ct));
    
}

BindlessObjectShaderGroup::BindlessObjectShaderGroup()
{
}

void BindlessObjectShaderGroup::push_back(FullShaderHandle o, FullShaderHandle s, FullShaderHandle d)
{
    opaqueShaders.push_back(o);
    shadowShaders.push_back(s);
    depthOnlyShaders.push_back(d);
}
