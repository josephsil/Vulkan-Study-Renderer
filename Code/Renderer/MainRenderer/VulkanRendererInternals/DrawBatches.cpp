#include "DrawBatches.h"

#include "engineGlobals.h"
#include "Renderer/gpu-data-structs.h"
#include "Renderer/RendererInterface.h"
#include "Renderer/MainRenderer/VulkanRenderer.h"
#include "Renderer/MeshCreation/meshData.h"
#include "Scene/AssetManager.h"
#include "Scene/Scene.h"

#pragma region draw batch TODO MOVE

struct pipelineBucket
{
    uint32_t pipelineIDX;
    Array<uint32_t> firstDrawIndices; //todo js
    Array<uint32_t> subMeshIndices; //todo js 
};


std::span<RenderBatch> RenderBatchQueue::AddBatch(
    CommonRenderPassData* context,
    RenderBatchCreationConfig passCreationConfig)
{
    batchConfigs.emplace_back(passCreationConfig.name, context, passCreationConfig);
    submeshCount += passCreationConfig.subMeshCount;
    return std::span<RenderBatch>(&batchConfigs.back(), 1);
}

std::span<RenderBatch> RenderBatchQueue::AddBatches(CommonRenderPassData* context,
                                                    std::span<RenderBatchCreationConfig> configs)
{
    auto start = batchConfigs.size();
    for (auto c :  configs)
    {
        batchConfigs.emplace_back(c.name, context, c);
        submeshCount += c.subMeshCount;
        drawCount += c.drawCount;
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
    //Configure frustum culling compute shader for this batch
    cullPConstants* cullconstants = MemoryArena::Alloc<cullPConstants>(context->tempAllocator);
    *cullconstants = {.view = passCreationConfig.cameraViewProjForCulling.view, .firstDraw = passCreationConfig.drawOffset, .frustumIndex = cullFrustumIndex, .objectCount = passCreationConfig.drawCount};

    ComputeCullListInfo* cullingInfo = MemoryArena::Alloc<ComputeCullListInfo>(context->tempAllocator);
    *cullingInfo =  {.firstDrawIndirectIndex = passCreationConfig.drawOffset, .drawCount =  passCreationConfig.drawCount,
        .viewMatrix = passCreationConfig.cameraViewProjForCulling.view,
        .projMatrix = passCreationConfig.cameraViewProjForCulling.proj, 
        .layout =context->cullLayout, 
       .pushConstantInfo =  {.ptr = cullconstants, .size =  sizeof(cullPConstants) }};


    
    ////Flatten buckets into simpleMeshPassInfos sorted by pipeline
    ///    //passes are bucketed by pipeline 
    Array<pipelineBucket> batchedDrawBuckets = MemoryArena::AllocSpan<pipelineBucket>(context->tempAllocator, passCreationConfig.shadersSupportedByBatch.size());
    for (uint32_t i = 0; i <passCreationConfig.shadersSupportedByBatch.size(); i++)
    {
        //Prepare a bucket for each shader
        if (passCreationConfig.shadersSupportedByBatch[i].shader >= batchedDrawBuckets.size())
        {
            batchedDrawBuckets.push_back( {i, Array(MemoryArena::AllocSpan<uint32_t>(context->tempAllocator, MAX_DRAWS_PER_PIPELINE))});
            batchedDrawBuckets.back().subMeshIndices = MemoryArena::AllocSpan<uint32_t>(context->tempAllocator,  passCreationConfig.subMeshCount);
            batchedDrawBuckets.back().firstDrawIndices = MemoryArena::AllocSpan<uint32_t>(context->tempAllocator,  passCreationConfig.subMeshCount);
        }
    }
    
    //Bucket submeshes by shader 
    uint32_t submeshIndex = 0;
    uint32_t meshletIndex = 0;
    for (uint32_t i = 0; i < context->scenePtr->ObjectsCount(); i++)
    {
        for (uint32_t j = 0; j <context->scenePtr->objects.subMeshMaterials[i].size(); j++)
        {
            uint32_t shaderGroupIndex = context->assetDataPtr->materials[context->scenePtr->objects.subMeshMaterials[i][j]].shaderGroupIndex; 
            uint32_t pipelineIndex = (uint32_t)passCreationConfig.shadersSupportedByBatch[shaderGroupIndex].shader;
            batchedDrawBuckets[pipelineIndex].subMeshIndices.push_back( context->scenePtr->allSubmeshes[submeshIndex]);
            batchedDrawBuckets[pipelineIndex].firstDrawIndices.push_back(meshletIndex);
            meshletIndex += (uint32_t)context->assetDataPtr->perSubmeshData[context->scenePtr->allSubmeshes[submeshIndex]].mesh->meshletsIndices.size();
            submeshIndex++;
        }
    }
    
    Array outputMeshPasses = MemoryArena::AllocSpan<simpleMeshPassInfo>(context->tempAllocator, MAX_PIPELINES);
    uint32_t drawOffset = passCreationConfig.drawOffset; //Where in the drawindirect buffer are we appending

    //Convert our output buckets into simplemeshpassinfo structs that later stages of the render can read
    for (size_t i = 0; i < batchedDrawBuckets.size(); i++)
    {
        auto bucketedBatch =  batchedDrawBuckets[i];
        if (bucketedBatch.firstDrawIndices.size() == 0)
        {
            continue;
        }
        auto shaderID =passCreationConfig.shadersSupportedByBatch[bucketedBatch.pipelineIDX];
        uint32_t meshletDrawsInBatch = 0;
        for (auto& submesh : bucketedBatch.subMeshIndices.getSpan())
        {
            //TODO JS MESHLETS
            meshletDrawsInBatch += (uint32_t)context->assetDataPtr->perSubmeshData[submesh].mesh->meshletsIndices.size();
        }

        drawCommandsPassOffset offset = {
            .firstDrawIndex = drawOffset,
        };
        outputMeshPasses.push_back({
            .offset = offset, .drawCount = meshletDrawsInBatch,
            .shader = shaderID,
            .sortedSubmeshes = bucketedBatch.subMeshIndices.getSpan(),
            .sortedfirstIndices = bucketedBatch.firstDrawIndices.getSpan()
        });

        drawOffset += meshletDrawsInBatch;
    }
    auto s = std::string(name);
    
 
      debugName =  s;
      drawRenderStepContext =  passCreationConfig.StepContext;
      computeRenderStepContext = context->computeRenderStepContext;
      pipelineLayoutGroup =   passCreationConfig.layoutGroup;
      indexBuffer = context->indexBuffer;
      indexBufferType =  VK_INDEX_TYPE_UINT32;
      perPipelinePasses = outputMeshPasses.getSpan();
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
