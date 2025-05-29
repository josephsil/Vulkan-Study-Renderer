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



RenderBatch CreateRenderBatch(CommonRenderPassData* context,
    RenderPassConfig config, RenderPassDrawData passInfo, bool shadow,
                                     PipelineLayoutHandle pipelineGroup,
                                    std::span<FullShaderHandle> shaderIDs, const char* name)
{
    
    ////Flatten buckets into simpleMeshPassInfos sorted by pipeline
    ///    //passes are bucketed by pipeline 
    Array<pipelineBucket> batchedDrawBuckets = MemoryArena::AllocSpan<pipelineBucket>(context->tempAllocator, shaderIDs.size());
    for (uint32_t i = 0; i <shaderIDs.size(); i++)
    {
        //Prepare a bucket for each shader
        if (shaderIDs[i].shader >= batchedDrawBuckets.size())
        {
            batchedDrawBuckets.push_back( {i, Array(MemoryArena::AllocSpan<uint32_t>(context->tempAllocator, MAX_DRAWS_PER_PIPELINE))});
            batchedDrawBuckets.back().subMeshIndices = MemoryArena::AllocSpan<uint32_t>(context->tempAllocator,  passInfo.subMeshcount);
            batchedDrawBuckets.back().firstDrawIndices = MemoryArena::AllocSpan<uint32_t>(context->tempAllocator,  passInfo.subMeshcount);
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
            uint32_t pipelineIndex = (uint32_t)shaderIDs[shaderGroupIndex].shader;
            batchedDrawBuckets[pipelineIndex].subMeshIndices.push_back( context->scenePtr->allSubmeshes[submeshIndex]);
            batchedDrawBuckets[pipelineIndex].firstDrawIndices.push_back(meshletIndex);
            meshletIndex += (uint32_t)context->assetDataPtr->meshData.perSubmeshData[context->scenePtr->allSubmeshes[submeshIndex]].meshletCt;
            submeshIndex++;
        }
    }
    
    Array outputMeshPasses = MemoryArena::AllocSpan<simpleMeshPassInfo>(context->tempAllocator, MAX_PIPELINES);
    uint32_t drawOffset = passInfo.drawOffset; //Where in the drawindirect buffer are we appending

    //Convert our output buckets into simplemeshpassinfo structs that later stages of the render can read
    for (size_t i = 0; i < batchedDrawBuckets.size(); i++)
    {
        auto bucketedBatch =  batchedDrawBuckets[i];
        if (bucketedBatch.firstDrawIndices.size() == 0)
        {
            continue;
        }
        auto shaderID = shaderIDs[bucketedBatch.pipelineIDX];
        uint32_t meshletDrawsInBatch = 0;
        for (auto& submesh : bucketedBatch.subMeshIndices.getSpan())
        {
            meshletDrawsInBatch += (uint32_t)context->assetDataPtr->meshData.perSubmeshData[submesh].meshletCt;
        }

        drawCommandsPassOffset offset = {
            .firstDrawIndex = drawOffset,
        };
        outputMeshPasses.push_back({
            .offset = offset,
            .drawCount = meshletDrawsInBatch,
            .shader = shaderID,
            .sortedSubmeshes = bucketedBatch.subMeshIndices.getSpan(),
            .sortedfirstIndices = bucketedBatch.firstDrawIndices.getSpan()
        });

        drawOffset += meshletDrawsInBatch;
    }
    
 return RenderBatch{
      .debugName =  name,
      .pipelineLayoutGroup =   pipelineGroup,
      .perPipelinePasses = outputMeshPasses.getSpan(),
      .depthAttatchment = config.depthAttatchment,
      .colorattatchment = config.colorAttatchment,
      .renderingAttatchmentExtent = config.extents,
      .matrices =  {passInfo.view, passInfo.proj},
      .pushConstants = config.PushConstants.ptr,
      .pushConstantsSize = config.PushConstants.size,
      //TODO JS: dynamically set bias per shadow caster, especially for cascades
      .depthBiasSetting =!shadow ?depthBiasSettng{.use =false, .depthBias = 0, .slopeBias = 0} : depthBiasSettng{.use = true, .depthBias = -8.0, .slopeBias = -10.0} 
          };
}

#pragma endregion
