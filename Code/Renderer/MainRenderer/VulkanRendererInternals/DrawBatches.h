#pragma once
#include "General/MemoryArena.h"
#include "Renderer/rendererGlobals.h"
#include "Renderer/MainRenderer/rendererStructs.h"
#include "Renderer/VulkanIncludes/forward-declarations-renderer.h"

struct ActiveRenderStepData;
struct PipelineLayoutManager;
class AssetManager;
struct Scene;
struct RenderBatch;
 

struct CommonRenderPassData 
{
    //name <- not common
    Allocator tempAllocator;
    Scene* scenePtr;
    AssetManager* assetDataPtr;
    VkPipelineLayout cullLayout; //todo
    PipelineLayoutManager* pipelineManagerPtr;
    ActiveRenderStepData* computeRenderStepContext; //todo

    VkBuffer indexBuffer;


};
struct RenderBatchCreationConfig
{
    char* name;
    renderPassAttatchmentInfo attatchmentInfo;
    std::span<FullShaderHandle> shaderGroup;
    PipelineLayoutHandle layoutGroup;
    ActiveRenderStepData* StepContext;
    pointerSize pushConstant;
    viewProj cameraViewProjForCulling;
    uint32_t drawOffset;
    uint32_t objectCount;
    depthBiasSettng depthBiasConfig;
    
};
struct RenderBatchQueue
{
    size_t drawCount;
    std::vector<RenderBatch> batchConfigs;
    void EmplaceBatch(const char* name,
                       CommonRenderPassData* context,
                       RenderBatchCreationConfig passCreationConfig);
};

struct RenderBatch
{
    std::string debugName;
    ActiveRenderStepData* drawRenderStepContext;
    ActiveRenderStepData* computeRenderStepContext;
    PipelineLayoutHandle pipelineLayoutGroup; 
    VkBuffer indexBuffer; 
    VkIndexType indexBufferType; // VK_INDEX_TYPE_UINT32
    std::span<simpleMeshPassInfo> meshPasses;
    ComputeCullListInfo* computeCullingInfo;
    VkRenderingAttachmentInfoKHR* depthAttatchment;
    VkRenderingAttachmentInfoKHR* colorattatchment; //nullptr
    VkExtent2D renderingAttatchmentExtent; //SHADOW_MAP_SIZE
    viewProj matrices;
    void* pushConstants; //shadowPushConstants constants;
    uint32_t pushConstantsSize;
    depthBiasSettng depthBiasSetting;
    RenderBatch(const char* name,
    CommonRenderPassData* context,
    RenderBatchCreationConfig passCreationConfig);
};
