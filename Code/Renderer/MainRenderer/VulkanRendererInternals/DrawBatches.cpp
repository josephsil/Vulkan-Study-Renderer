#include "DrawBatches.h"

#include "engineGlobals.h"
#include "Renderer/gpu-data-structs.h"
#include "Renderer/RendererInterface.h"
#include "Scene/AssetManager.h"
#include "Scene/Scene.h"

#pragma region draw batch TODO MOVE

struct pipelineBucket
{
    uint32_t pipelineIDX;
    Array<uint32_t> objectIndices; //todo js
    Array<uint32_t> subMeshIndices; //todo js -- make subMeshMeshletInfo
};


std::span<RenderBatch> RenderBatchQueue::AddBatch(
    CommonRenderPassData* context,
    RenderBatchCreationConfig passCreationConfig)
{
    batchConfigs.emplace_back(passCreationConfig.name, context, passCreationConfig);
    drawCount += passCreationConfig.objectCount;
    return std::span<RenderBatch>(&batchConfigs.back(), 1);
}

std::span<RenderBatch> RenderBatchQueue::AddBatches(CommonRenderPassData* context,
                                                    std::span<RenderBatchCreationConfig> configs)
{
    auto start = batchConfigs.size();
    for (auto c :  configs)
    {
        batchConfigs.emplace_back(c.name, context, c);
        drawCount += c.objectCount;
    }
    return {&batchConfigs[start], configs.size()};
}

RenderBatch::RenderBatch(const char* name, CommonRenderPassData* context, RenderBatchCreationConfig passCreationConfig)
{


    //Culling override code
    uint32_t cullFrustumIndex = (passCreationConfig.drawOffset) * 6;
    if (debug_cull_override)
    {
        cullFrustumIndex =  debug_cull_override_index * 6;
    }
    ///todo js MESHLET PERF: This is happening *per meshlet*, most only needs to happen *per submesh*
    //todo js I can reduce the number of materials back down to the real number, and use`subMeshMeshletInfo` to operate over spans of submeshct instead of meshletct
    // printf("TODO: batch draws %d objects. \n This is a combination of issues -- need to compact, and need to treat meshlets differntly than submeshes to avoid passing around giant spans\n  ",   passCreationConfig.objectCount );
    //Configure frustum culling compute shader for this batch
    cullPConstants* cullconstants = MemoryArena::Alloc<cullPConstants>(context->tempAllocator);
    *cullconstants = {.view = passCreationConfig.cameraViewProjForCulling.view, .firstDraw = passCreationConfig.drawOffset, .frustumIndex = cullFrustumIndex, .objectCount = (uint32_t)context->assetDataPtr->meshletCount};

    ComputeCullListInfo* cullingInfo = MemoryArena::Alloc<ComputeCullListInfo>(context->tempAllocator);
    *cullingInfo =  {.firstDrawIndirectIndex = passCreationConfig.drawOffset, .drawCount =  (uint32_t)context->assetDataPtr->meshletCount,
        .viewMatrix = passCreationConfig.cameraViewProjForCulling.view,
        .projMatrix = passCreationConfig.cameraViewProjForCulling.proj, 
        .layout =context->cullLayout, 
       .pushConstantInfo =  {.ptr = cullconstants, .size =  sizeof(cullPConstants) }};


    
    ////Flatten buckets into simpleMeshPassInfos sorted by pipeline
    ///    //passes are bucketed by pipeline 
    //initialize and fill pipeline buckets
    Array<pipelineBucket> batchedDrawBuckets = MemoryArena::AllocSpan<pipelineBucket>(context->tempAllocator, passCreationConfig.shadersSupportedByBatch.size());
    for (uint32_t i = 0; i <passCreationConfig.shadersSupportedByBatch.size(); i++)
    {
        //Prepare a bucket for each shader
        //Note: not obvious to me why I did this this way, with the >= size 
        if (passCreationConfig.shadersSupportedByBatch[i].shader >= batchedDrawBuckets.size())
        {
            batchedDrawBuckets.push_back( {i, Array(MemoryArena::AllocSpan<uint32_t>(context->tempAllocator, MAX_DRAWS_PER_PIPELINE))});
            batchedDrawBuckets.back().subMeshIndices = MemoryArena::AllocSpan<uint32_t>(context->tempAllocator,  passCreationConfig.objectCount);
            batchedDrawBuckets.back().objectIndices = MemoryArena::AllocSpan<uint32_t>(context->tempAllocator,  passCreationConfig.objectCount);
        }
    }
    
    //Bucket submeshes by shader 
    uint32_t submeshIndex = 0;
    for (uint32_t i = 0; i < context->scenePtr->ObjectsCount(); i++)
    {
        for (uint32_t j = 0; j <context->scenePtr->objects.subMeshMaterials[i].size(); j++)
        {
            uint32_t shaderGroupIndex = context->assetDataPtr->materials[context->scenePtr->objects.subMeshMaterials[i][j]].shaderGroupIndex; 
            uint32_t pipelineIndex = (uint32_t)passCreationConfig.shadersSupportedByBatch[shaderGroupIndex].shader;
            batchedDrawBuckets[pipelineIndex].subMeshIndices.push_back(submeshIndex++);
            batchedDrawBuckets[pipelineIndex].objectIndices.push_back(i);
        }
    }
    
    Array outputMeshPasses = MemoryArena::AllocSpan<simpleMeshPassInfo>(context->tempAllocator, MAX_PIPELINES);
    uint32_t drawOffset = passCreationConfig.drawOffset; //Where in the drawindirect buffer are we appending

    //Convert our output buckets into simplemeshpassinfo structs that later stages of the render can read
    for (size_t i = 0; i < batchedDrawBuckets.size(); i++)
    {
        auto bucketedBatch =  batchedDrawBuckets[i];
        if (bucketedBatch.objectIndices.size() == 0)
        {
            continue;
        }
        std::span<unsigned int> objectIndices = bucketedBatch.objectIndices.getSpan();
        auto shaderID =passCreationConfig.shadersSupportedByBatch[bucketedBatch.pipelineIDX];
        auto firstIndex =drawOffset;
        uint32_t drawsInBatch = 0;
        for (auto& submesh : bucketedBatch.subMeshIndices.getSpan())
        {
            //TODO JS MESHLETS
            drawsInBatch+= (uint32_t)context->assetDataPtr->perSubmeshData[submesh].meshlets.size();
        }
      
        outputMeshPasses.push_back({.firstIndex = firstIndex, .ct = drawsInBatch, .shader = shaderID});

        drawOffset += drawsInBatch;
    }
    auto s = std::string(name);
    
 
      debugName =  s;
      drawRenderStepContext =  passCreationConfig.StepContext;
      computeRenderStepContext = context->computeRenderStepContext;
      pipelineLayoutGroup =   passCreationConfig.layoutGroup;
      indexBuffer = context->indexBuffer;
      indexBufferType =  VK_INDEX_TYPE_UINT32;
      subMeshPasses = outputMeshPasses.getSpan();
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
