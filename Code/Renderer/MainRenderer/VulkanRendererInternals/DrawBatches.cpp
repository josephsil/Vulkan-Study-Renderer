#include "DrawBatches.h"

#include "Renderer/gpu-data-structs.h"
#include "Renderer/RendererInterface.h"
#include "Scene/AssetManager.h"
#include "Scene/Scene.h"

#pragma region draw batch TODO MOVE

struct pipelineBucket
{
    uint32_t pipelineIDX;
    Array<uint32_t> objectIndices; 
};


void RenderBatchQueue::EmplaceBatch(const char* name,
    CommonRenderPassData* context,
    RenderBatchCreationConfig passCreationConfig )
{
    batchConfigs.emplace_back(name, context, passCreationConfig);
    drawCount += passCreationConfig.objectCount;
}

RenderBatch::RenderBatch(const char* name, CommonRenderPassData* context, RenderBatchCreationConfig passCreationConfig)
{
    
    uint32_t cullFrustumIndex = (passCreationConfig.drawOffset) * 6;
    if (debug_cull_override)
    {
        cullFrustumIndex =  debug_cull_override_index * 6;
    }
        
    cullPConstants* cullconstants = MemoryArena::Alloc<cullPConstants>(context->tempAllocator);


    *cullconstants = {.view = passCreationConfig.cameraViewProjForCulling.view, .firstDraw = passCreationConfig.drawOffset, .frustumIndex = cullFrustumIndex, .objectCount = passCreationConfig.objectCount};
    ComputeCullListInfo* cullingInfo = MemoryArena::Alloc<ComputeCullListInfo>(context->tempAllocator);
    *cullingInfo =  {.firstDrawIndirectIndex = passCreationConfig.drawOffset, .drawCount = passCreationConfig.objectCount, .viewMatrix = passCreationConfig.cameraViewProjForCulling.view, .projMatrix = passCreationConfig.cameraViewProjForCulling.proj, 
        .layout =context->cullLayout, // just get pipeline layout globally?
       .pushConstantInfo =  {.ptr = cullconstants, .size =  sizeof(cullPConstants) }};

    Array<pipelineBucket> batchedDrawBuckets = MemoryArena::AllocSpan<pipelineBucket>(context->tempAllocator, passCreationConfig.shaderGroup.size());
    ////Flatten buckets into simpleMeshPassInfos sorted by pipeline
    ///    //Opaque passes are bucketed by pipeline 
    //initialize and fill pipeline buckets
    for (uint32_t i = 0; i <passCreationConfig.shaderGroup.size(); i++)
    {
        if (passCreationConfig.shaderGroup[i].shader >= batchedDrawBuckets.size())
        {
            batchedDrawBuckets.push_back( {i, Array(MemoryArena::AllocSpan<uint32_t>(context->tempAllocator, MAX_DRAWS_PER_PIPELINE))});
        }
    }
    for (uint32_t i = 0; i < passCreationConfig.objectCount; i++)
    {
        uint32_t shaderGroupIndex = context->assetDataPtr->materials[context->scenePtr->objects.materials[i]].shaderGroupIndex;
        uint32_t pipelineIndex = (uint32_t)passCreationConfig.shaderGroup[shaderGroupIndex].shader;
        batchedDrawBuckets[pipelineIndex].objectIndices.push_back(i);
    }
    
    Array _meshPasses = MemoryArena::AllocSpan<simpleMeshPassInfo>(context->tempAllocator, MAX_PIPELINES);
    uint32_t drawOffsetAtTheStartOfOpaque = passCreationConfig.drawOffset;
    uint32_t subspanOffset = 0;
    for (size_t i = 0; i < batchedDrawBuckets.size(); i++)
    {
        auto drawBatch =  batchedDrawBuckets[i];
        if (drawBatch.objectIndices.size() == 0)
        {
            continue;
        }
        std::span<unsigned int> objectIndices = drawBatch.objectIndices.getSpan();
        auto firstIndex = drawOffsetAtTheStartOfOpaque + subspanOffset;

        auto shaderID =passCreationConfig.shaderGroup[drawBatch.pipelineIDX];

        uint32_t drawCount = (uint32_t)context->scenePtr->GetTotalSubmeshesForObjects(objectIndices); //One draw for each submesh in our object indices count
        
        _meshPasses.push_back({.firstIndex = firstIndex, .ct = drawCount, .pipeline = context->pipelineManagerPtr->GetPipeline(shaderID.layout, shaderID.shader),
            .sortedObjectIDs =  drawBatch.objectIndices.getSpan()});

        subspanOffset += drawCount;
    }
    auto s = std::string(name);
    
 
      debugName =  s;
      drawRenderStepContext =  passCreationConfig.StepContext;
      computeRenderStepContext = context->computeRenderStepContext;
      pipelineLayoutGroup =   passCreationConfig.layoutGroup;
      indexBuffer = context->indexBuffer;
      indexBufferType =  VK_INDEX_TYPE_UINT32;
      meshPasses = _meshPasses.getSpan();
      computeCullingInfo = cullingInfo;
      depthAttatchment = passCreationConfig.attatchmentInfo.depthDraw;
      colorattatchment = passCreationConfig.attatchmentInfo.colorDraw;
      renderingAttatchmentExtent = passCreationConfig.attatchmentInfo.extents;
      matrices =  passCreationConfig.cameraViewProjForCulling;
      pushConstants = passCreationConfig.pushConstant.ptr;
      pushConstantsSize = passCreationConfig.pushConstant.size;
      //TODO JS: dynamically set bias per shadow caster, especially for cascades
      depthBiasSetting = passCreationConfig.depthBiasConfig;
    
}

#pragma endregion
