#pragma once
#include "General/MemoryArena.h"
#include "Renderer/rendererGlobals.h"
#include "Renderer/RendererSharedTypes.h"
#include "Renderer/MainRenderer/rendererStructs.h"
#include "Renderer/MainRenderer/PipelineManager/PipelineLayoutManager.h"
#include "Renderer/VulkanIncludes/forward-declarations-renderer.h"

struct ActiveRenderStepData;
struct PipelineLayoutManager;
class AssetManager;
struct Scene;
struct RenderBatch;
 

struct CommonRenderPassData 
{
    //name <- not common
    ArenaAllocator tempAllocator;
    Scene* scenePtr;
    AssetManager* assetDataPtr;
    VkPipelineLayout cullLayout; //todo get from a lookup
};
struct RenderBatchCreationConfig
{
    char* name;
    renderPassAttatchmentInfo attatchmentInfo;
    std::span<FullShaderHandle> shadersSupportedByBatch;
    PipelineLayoutHandle layoutGroup;
    pointerSize pushConstant;
    viewProj cameraViewProjForCulling;
    uint32_t drawOffset;
    uint32_t subMeshCount;
    uint32_t drawCount;
    depthBiasSettng depthBiasConfig;
    
};
struct RenderBatchQueue
{
    size_t submeshCount;
    size_t totalDrawCount;
    std::vector<RenderBatch> batchConfigs;
    std::span<RenderBatch> AddBatch(CommonRenderPassData* context, RenderBatchCreationConfig passCreationConfig);
    std::span<RenderBatch> AddBatches(CommonRenderPassData* context, std::span<RenderBatchCreationConfig> configs);
};

struct RenderBatch
{
    std::string debugName;
    PipelineLayoutHandle pipelineLayoutGroup; 
    std::span<simpleMeshPassInfo> perPipelinePasses;
    ComputeCullListInfo* computeCullingInfo;
    VkRenderingAttachmentInfoKHR* depthAttatchment;
    VkRenderingAttachmentInfoKHR* colorattatchment; 
    VkExtent2D renderingAttatchmentExtent;
    viewProj matrices;
    void* pushConstants; 
    uint32_t pushConstantsSize;
    depthBiasSettng depthBiasSetting;
    
    RenderBatch(const char* name,
    CommonRenderPassData* context,
    RenderBatchCreationConfig passCreationConfig);
};
