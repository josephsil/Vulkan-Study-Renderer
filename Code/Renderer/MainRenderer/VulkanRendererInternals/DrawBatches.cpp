#include "DrawBatches.h"

#include "Renderer/gpu-data-structs.h"
#include "Renderer/RendererInterface.h"
#include "Scene/AssetManager.h"
#include "Scene/Scene.h"

#pragma region draw batch TODO MOVE

struct pipelineBucket
{
    uint32_t pipelineIDX;
    Array<uint32_t> objectIndices; //todo js
    Array<uint32_t> subMeshIndices; //todo js 
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
            batchedDrawBuckets.back().subMeshIndices = MemoryArena::AllocSpan<uint32_t>(context->tempAllocator,  passCreationConfig.objectCount);
            batchedDrawBuckets.back().objectIndices = MemoryArena::AllocSpan<uint32_t>(context->tempAllocator,  passCreationConfig.objectCount);
        }
    }
    uint32_t submeshIndex = 0;
    uint32_t objectIndex = 0;
    for (uint32_t i = 0; i < context->scenePtr->ObjectsCount(); i++)
    {
        for (uint32_t j = 0; j <context->scenePtr->objects.materialsForSubmeshes[i].size(); j++)
        {
            uint32_t shaderGroupIndex = context->assetDataPtr->materials[context->scenePtr->objects.materialsForSubmeshes[i][j]].shaderGroupIndex; //TODO JS: takes the shader from the first submesh of the object! need to break transforms out and loop submesh ct here
            uint32_t pipelineIndex = (uint32_t)passCreationConfig.shaderGroup[shaderGroupIndex].shader;
            batchedDrawBuckets[pipelineIndex].subMeshIndices.push_back(submeshIndex++);
            batchedDrawBuckets[pipelineIndex].objectIndices.push_back(objectIndex);
        }
        objectIndex++;
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

        _meshPasses.push_back({.firstIndex = firstIndex, .ct = (uint32_t)drawBatch.subMeshIndices.size(), .pipeline = context->pipelineManagerPtr->GetPipeline(shaderID.layout, shaderID.shader),
            .sortedObjectIDs =  drawBatch.subMeshIndices.getSpan()});

        subspanOffset += (uint32_t)drawBatch.subMeshIndices.size();
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
