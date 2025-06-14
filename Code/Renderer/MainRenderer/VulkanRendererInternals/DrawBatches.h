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
 
//RenderBatches are used to gather and submit sets of objects (divided by pipeline) for rendering
//This is a little convoluted, and happens in two parts. 
//Initially, RenderPassDrawData and CommonRenderPassData objects are created in VulkanRenderer.cpp
//Those contain the basic information about a 'render pass', which is like:
// 'this list of objects need to be submitted to be drawn in this frame' (for some pipeline layout and commandbuffer)

//Those objects are provided to CreateRenderBatch() along with the pipeline group and attatchment/target info, 
// and the list of shaderIDs in the draw 
//CreateRenderBatch then organizes and buckets the relevant draws by shader, 
//and bundles up all of the necessary data for each batch to build the drawindirect commands and submit the draws.
//This is fairly brittle and bug prone, there are a few layers of indirection between pass data, batches, and draw commands, 
// which all depend on indices being perfectly in sync

//This is another part of the renderer that was started when I initially began learning vulkan, and has been halfway refactored and extended. 
//Needs a real rework.
struct RenderPassDrawData
{
    uint32_t drawCount;
    uint32_t drawOffset;
    uint32_t subMeshcount;
    glm::mat4 proj;
    glm::mat4 view;
    float farPlane;
    float nearPlane;
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