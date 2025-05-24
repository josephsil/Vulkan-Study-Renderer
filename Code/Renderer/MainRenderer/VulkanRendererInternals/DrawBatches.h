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
 
struct RenderPassDrawData
{
    uint32_t drawCount;
    uint32_t drawOffset;
    uint32_t subMeshcount;
    glm::mat4 proj;
    glm::mat4 view;
};

struct RenderPassConfig
{
    pointerSize PushConstants;
    VkRenderingAttachmentInfoKHR* depthAttatchment;
    VkRenderingAttachmentInfoKHR* colorAttatchment;
    VkExtent2D extents;
};

struct CommonRenderPassData 
{
    //name <- not common
    ArenaAllocator tempAllocator;
    Scene* scenePtr;
    AssetManager* assetDataPtr;
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
    std::vector<RenderBatch> batchConfigs;
};

struct RenderBatch
{
    const char* debugName;
    PipelineLayoutHandle pipelineLayoutGroup; 
    std::span<simpleMeshPassInfo> perPipelinePasses;
    VkRenderingAttachmentInfoKHR* depthAttatchment;
    VkRenderingAttachmentInfoKHR* colorattatchment; 
    VkExtent2D renderingAttatchmentExtent;
    viewProj matrices;
    void* pushConstants; 
    uint32_t pushConstantsSize;
    depthBiasSettng depthBiasSetting;
    
   
};
RenderBatch CreateRenderBatch( CommonRenderPassData* context,
   RenderPassConfig config, RenderPassDrawData passInfo, bool shadow,
                                    PipelineLayoutHandle pipelineGroup,
                                   std::span<FullShaderHandle> shaderIDs, const char* name);