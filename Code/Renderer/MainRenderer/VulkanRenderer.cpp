#include "VulkanRenderer.h"
#include "vulkan/vulkan_core.h"
#undef main
#define SDL_MAIN_HANDLED 
#include <SDL2/SDL.h>
////
#include <cstdlib>
#include <span>
#include <Renderer/Vertex.h>
#include <ImageLibraryImplementations.h>
#include <General/Array.h>
#include <General/MemoryArena.h>
#include <General/GLM_impl.h>
#include <glm/gtx/matrix_decompose.hpp>

#include <Renderer/VulkanBuffers/bufferCreation.h>
#include <Renderer/CommandPoolManager.h>
#include <Renderer/gpu-data-structs.h>
#include <Renderer/MeshCreation/MeshData.h>
#include <Scene/AssetManager.h>
#include <imgui.h>
#include <Renderer/RendererInterface.h>
#include <Renderer/TextureCreation/Internal/TextureCreationUtilities.h>
#include <Renderer/TextureCreation/TextureData.h>
#include <Renderer/vulkan-utilities.h>
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_vulkan.h"
#include <Renderer/rendererGlobals.h>
#include <Scene/Scene.h>
#include <Scene/Transforms.h>
#include <Renderer/VulkanIncludes/vmaImplementation.h>
#include <Renderer/MainRenderer/VulkanRendererInternals/DrawBatches.h>
#include <Renderer/VulkanIncludes/Vulkan_Includes.h>
#include <Renderer/MainRenderer/VulkanRendererInternals/DebugLineData.h>
#include <Renderer/MainRenderer/VulkanRendererInternals/LightAndCameraHelpers.h>
#include <Renderer/MainRenderer/VulkanRendererInternals/RendererHelpers.h>

#include "General/LinearDictionary.h"
#include "General/ThreadedTextureLoading.h"
//Should move somewhere
#include <fmtInclude.h>
struct GPU_perShadowData;
std::vector<unsigned int> pastTimes;
unsigned int averageCbTime;
unsigned int frames;
unsigned int MAX_TEXTURES = 120;
uint32_t internal_debug_cull_override_index = 0;
TextureData cube_irradiance;
TextureData cube_specular;
auto debugLinesManager = DebugLineData();
FullShaderHandle OPAQUE_PREPASS_SHADER_INDEX = {SIZE_MAX,SIZE_MAX};
FullShaderHandle SHADOW_PREPASS_SHADER_INDEX = {SIZE_MAX,SIZE_MAX};
FullShaderHandle DEBUG_LINE_SHADER_INDEX = {SIZE_MAX,SIZE_MAX};
FullShaderHandle  DEBUG_RAYMARCH_SHADER_INDEX ={SIZE_MAX,SIZE_MAX};
//Slowly making this less of a giant class that owns lots of things and moving to these static FNS -- will eventually migrate thewm
RenderPassDrawData DebugCullingData[100];
LinearDictionary<FullShaderHandle, FullShaderHandle> PlaceholderDepthPassLookup = {};

PerSceneShadowResources AllocateShadowMemory(rendererObjects initializedrenderer,
                                                    MemoryArena::Allocator* allocationArena);
VkDescriptorSet UpdateAndGetBindlessDescriptorSetForFrame(PerThreadRenderContext context,
                                                          DescriptorDataForPipeline descriptorData, int currentFrame,
                                                          std::span<descriptorUpdates> updates);
size_t UpdateDrawCommandData(AssetManager* rendererData,
                                                 std::span<drawCommandData> targetDrawCommandSpan,
                                                 std::span<uint32_t> submeshIndex,
                                                 std::span<uint32_t> submeshFirstDrawIndex);

VulkanRenderer::VulkanRenderer()
{
  
    //Initialize memory arenas we use throughout renderer 
    MemoryArena::Initialize(&rendererArena, 1000000 * 200); // 200mb
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        MemoryArena::Initialize(&perFrameArenas[i], 1000000 * 120);
        //TODO JS: Could be much smaller if I used stable memory for per frame verts and stuff
    }




    opaqueObjectShaderSets = BindlessObjectShaderGroup(&rendererArena, 10);
    
    //Initialize the window and vulkan renderer
    initializeWindow();
    vulkanObjects = initializeVulkanObjects(_window, WIDTH, HEIGHT);

    //Initialize the main datastructures the renderer uses
    INIT_QUEUES(vulkanObjects.vkbdevice);



	this->deletionQueue = MemoryArena::AllocDefaultConstructor<RendererDeletionQueue>(&rendererArena);
	InitRendererDeletionQueue(deletionQueue, vulkanObjects.vkbdevice, vulkanObjects.vmaAllocator);
	assert(deletionQueue->device != VK_NULL_HANDLE);
	this->commandPoolmanager = MemoryArena::AllocConstruct<CommandPoolManager>(&rendererArena, vulkanObjects.vkbdevice, deletionQueue);

    perFrameData = MemoryArena::AllocSpan<FrameData>(&rendererArena, MAX_FRAMES_IN_FLIGHT);
    perFrameDeletionQueuse = MemoryArena::AllocSpanDefaultConstructor<RendererDeletionQueue>(&rendererArena, MAX_FRAMES_IN_FLIGHT); 
    //semaphores
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        CreateSemaphore(vulkanObjects.vkbdevice.device, &(perFrameData[i].perFrameSemaphores.swapchainSemaphore), "Per Frame swapchain ready semaphore", deletionQueue);
        CreateSemaphore(vulkanObjects.vkbdevice.device, &(perFrameData[i].perFrameSemaphores.prepassSemaphore), "Per Frame prepass finished semaphore", deletionQueue);
        CreateSemaphore(vulkanObjects.vkbdevice.device, &(perFrameData[i].perFrameSemaphores.opaqueSemaphore), "Per Frame opaque finished semaphore", deletionQueue);
        CreateSemaphore(vulkanObjects.vkbdevice.device, &(perFrameData[i].perFrameSemaphores.presentSemaphore), "Per Frame ready to present semaphore", deletionQueue);
        CreateSemaphore(vulkanObjects.vkbdevice.device, &(perFrameData[i].perFrameSemaphores.cullingSemaphore), "Per Frame finished culling semaphore", deletionQueue);
        CreateFence(vulkanObjects.vkbdevice.device,  &perFrameData[i].inFlightFence, "Per Frame finished rendering Fence", deletionQueue);
        CreateFence(vulkanObjects.vkbdevice.device,  &perFrameData[i].perFrameSemaphores.cullingFence, "Per frame culling fence", deletionQueue );
        vkResetFences(vulkanObjects.vkbdevice.device,  1, &perFrameData[i].perFrameSemaphores.cullingFence);
		InitRendererDeletionQueue(&perFrameDeletionQueuse[i], vulkanObjects.vkbdevice, vulkanObjects.vmaAllocator);
    }

	shadowResources = AllocateShadowMemory(vulkanObjects, &rendererArena);
	globalResources = static_initializeResources(vulkanObjects, &rendererArena, deletionQueue,commandPoolmanager);

    //Initialize sceneData
    AssetDataAndMemory = GetGlobalAssetManager();
    //imgui
    initializeDearIMGUI();
    pastTimes.resize(9999);
}


PerThreadRenderContext VulkanRenderer::GetMainRendererContext()
{
    return PerThreadRenderContext{
        .physicalDevice = vulkanObjects.vkbdevice.physical_device,
        .device =  vulkanObjects.vkbdevice.device,
        .textureCreationcommandPoolmanager = commandPoolmanager,
        .allocator =  vulkanObjects.vmaAllocator,
        .arena = &rendererArena,
        .tempArena = &perFrameArenas[currentFrame],
        .threadDeletionQueue = deletionQueue,
        .ImportFence = VK_NULL_HANDLE, //todo js
        .vkbd = &vulkanObjects.vkbdevice,
        };
}

BufferCreationContext VulkanRenderer::getPartialRendererContext()
{
    return objectCreationContextFromRendererContext(GetMainRendererContext());
}



void VulkanRenderer::UpdateShadowImageViews(int frame, SceneSizeData lightData)
{
       
    shadowResources.shadowMapSingleLayerViews[frame] =  MemoryArena::AllocSpan<VkImageView>(&rendererArena, MAX_SHADOWMAPS);
    shadowResources.WIP_shadowDepthPyramidInfos[frame] = MemoryArena::AllocSpan<DepthPyramidInfo>(&rendererArena, MAX_SHADOWMAPS);
    auto scratchMemory = GetScratchStringMemory();
    for (int j = 0; j < MAX_SHADOWMAPS; j++)
    {
        shadowResources.shadowMapSingleLayerViews[frame][j] = TextureUtilities::createImageViewCustomMip(
            getPartialRendererContext(), shadowResources.shadowImages[frame], shadowFormat,
            VK_IMAGE_ASPECT_DEPTH_BIT,
            (VkImageViewType)  VK_IMAGE_TYPE_2D,
            1, 
            j,1, 0);
        fmt::format_to_n(scratchMemory.begin(), scratchMemory.size(), "ShadowMap{}\0", j);
        SetDebugObjectName(vulkanObjects.vkbdevice.device, VK_OBJECT_TYPE_IMAGE_VIEW,scratchMemory.data(), (uint64_t)shadowResources.shadowMapSingleLayerViews[frame][j]);
     
    }
    for (int j = 0; j < MAX_SHADOWMAPS_WITH_CULLING; j++)
    {
        fmt::format_to_n(scratchMemory.begin(), scratchMemory.size(), "ShadowMapPyramid{}\0", j);
        shadowResources.WIP_shadowDepthPyramidInfos[frame][j] = 
            CreateDepthPyramidResources(vulkanObjects, &rendererArena, scratchMemory.data(), deletionQueue, 
                                              commandPoolmanager);
    }

    shadowResources.shadowMapTextureArrayViews[frame] =  MemoryArena::AllocSpan<VkImageView>(&rendererArena, MAX_SHADOWCASTERS);
    for (size_t j = 0; j < MAX_SHADOWCASTERS; j++)
    {
        if (j < glm::min(lightData.lightCount, MAX_SHADOWCASTERS))
        {
            VkImageViewType type = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
            uint32_t ct = (uint32_t)shadowCountFromLightType(lightData.lightTypes[j]);
            if (lightData.lightTypes[j] == LIGHT_POINT)
            {
                type = VK_IMAGE_VIEW_TYPE_CUBE;
            }
           
            shadowResources.shadowMapTextureArrayViews[frame][j] = TextureUtilities::createImageViewCustomMip(
                getPartialRendererContext(), shadowResources.shadowImages[frame], shadowFormat,
                VK_IMAGE_ASPECT_DEPTH_BIT,
                (VkImageViewType)  type,
                ct, 
                (uint32_t)Scene::getShadowDataIndex(j, lightData.lightTypes), 1, 0);

         
        }
        else
        {
            shadowResources.shadowMapTextureArrayViews[frame][j] = shadowResources.shadowMapSingleLayerViews[frame][j];
        }
    }
}


static PerSceneShadowResources AllocateShadowMemory(rendererObjects initializedrenderer,  MemoryArena::Allocator* allocationArena)
{
  
    auto shadowImages = MemoryArena::AllocSpan<VkImage>(allocationArena, MAX_FRAMES_IN_FLIGHT); 
    auto shadowSamplers = MemoryArena::AllocSpan<VkSampler>(allocationArena, MAX_FRAMES_IN_FLIGHT);
    auto shadowSamplersWithMips = MemoryArena::AllocSpan<VkSampler>(allocationArena, MAX_FRAMES_IN_FLIGHT);
    auto shadowMemory = MemoryArena::AllocSpan<VmaAllocation>(allocationArena, MAX_FRAMES_IN_FLIGHT);
    auto shadowMapRenderingImageViews = MemoryArena::AllocSpan<std::span<VkImageView>>(allocationArena, MAX_FRAMES_IN_FLIGHT); 
    auto shadowMapSamplingImageViews = MemoryArena::AllocSpan<std::span<VkImageView>>(allocationArena, MAX_FRAMES_IN_FLIGHT);
    auto shadowMapDepthImageInfos = MemoryArena::AllocSpan<std::span<DepthPyramidInfo>>(allocationArena, MAX_FRAMES_IN_FLIGHT); 

    return {
        
        .shadowMapSingleLayerViews = (shadowMapRenderingImageViews),
        .shadowMapTextureArrayViews =  (shadowMapSamplingImageViews),
        .shadowSamplers =shadowSamplers,
        .shadowImages =shadowImages,
        .WIP_shadowDepthPyramidInfos = shadowMapDepthImageInfos,
        .shadowMemory = shadowMemory,
    };
}


//Pipelines. Currently, the assumption is that the renderer will (continue to) have a very small number of pipeline *layouts*,
//And a fairly small number of pipelines total. Binding is expected to happen at very regular times in the frame.
//So right now pipeline layout and configuration is mostly hardcoded. The setup is a little verbose and repetitive.
//Pipeline group (see PipelineLayoutGroup.h) creation starts with a span of DescriptorLayouts, which are used to 
//Create the VkDescriptorSet, and the DescriptorDataForPipeline struct with info for my code. 
//For the bindless layouts which will get the same updates every frame, I also call functions to create std::span<DescriptorUpdateData>
//DescriptorUpdateData are typed objects used to create VkDescriptorInfo objects when we do updates.
//Other pipelines typically create these in place while rendering the frame.

//After layouts, I create PipelineGroups (see PipelineLayoutGroup.h), for each layout, and call CreatePipeline for each shader/pipeline permuation;
//Finally, some lookups/global IDs are updated to for later rendering code to use.
void VulkanRenderer::InitializePipelines(size_t shadowCasterCount)
{
    
    CreateDescriptorSetPool(GetMainRendererContext(), &descriptorPool);

    VkDescriptorSetLayout perSceneBindlessDescriptorLayout;
    VkDescriptorSetLayout perFrameBindlessLayout;
    
    VkDescriptorSetLayoutBinding sceneLayoutBindings[6] = {};
    sceneLayoutBindings[0] = VkDescriptorSetLayoutBinding{0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, MAX_TEXTURES, VK_SHADER_STAGE_FRAGMENT_BIT,  VK_NULL_HANDLE};// images 1 //per scene
    sceneLayoutBindings[1] = VkDescriptorSetLayoutBinding{1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,2, VK_SHADER_STAGE_FRAGMENT_BIT,  VK_NULL_HANDLE};//  cubes 2 //per scene?
    sceneLayoutBindings[2] = VkDescriptorSetLayoutBinding{2, VK_DESCRIPTOR_TYPE_SAMPLER, MAX_TEXTURES, VK_SHADER_STAGE_FRAGMENT_BIT , VK_NULL_HANDLE} ;// iamges  4  // perscene
    sceneLayoutBindings[3] = VkDescriptorSetLayoutBinding{3, VK_DESCRIPTOR_TYPE_SAMPLER, 2, VK_SHADER_STAGE_FRAGMENT_BIT, VK_NULL_HANDLE}  ;// iamges 5  // perscen
    sceneLayoutBindings[4] = VkDescriptorSetLayoutBinding{4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, VK_NULL_HANDLE} ; //Geometry//  // mesh 7 //per scene, for now
    sceneLayoutBindings[5] = VkDescriptorSetLayoutBinding{5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, VK_NULL_HANDLE} ;  //11 vert buffer info -- per scene, for now

    perSceneBindlessDescriptorLayout = DescriptorSets::createVkDescriptorSetLayout(GetMainRendererContext(), sceneLayoutBindings, "Per Scene Bindlses Layout");
    auto perSceneBindlessDescriptorData = DescriptorSets::CreateDescriptorDataForPipeline(GetMainRendererContext(), perSceneBindlessDescriptorLayout, false, sceneLayoutBindings, "Per Scene Bindless Set", descriptorPool);
    perSceneDescriptorUpdates = CreatePerSceneDescriptorUpdates(0, &rendererArena, perSceneBindlessDescriptorData.layoutBindings);

    VkDescriptorSetLayoutBinding frameLayoutBindings[7] = {};
    frameLayoutBindings[0] = VkDescriptorSetLayoutBinding{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL, VK_NULL_HANDLE}; //Globals  0 // per frame
    frameLayoutBindings[1] = VkDescriptorSetLayoutBinding{1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, MAX_SHADOWMAPS, VK_SHADER_STAGE_FRAGMENT_BIT  }; //SHADOW//  //shadow images 3 //per scene
    frameLayoutBindings[2] = VkDescriptorSetLayoutBinding{2, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_NULL_HANDLE}  ; // shadow iamges  6  // perscene
    frameLayoutBindings[3] = VkDescriptorSetLayoutBinding{3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,  VK_SHADER_STAGE_FRAGMENT_BIT, VK_NULL_HANDLE} ; //light//   //8 light info -- per frame
    frameLayoutBindings[4] = VkDescriptorSetLayoutBinding{4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,  VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, VK_NULL_HANDLE} ;  //9 Object info -- per frame
    frameLayoutBindings[5] = VkDescriptorSetLayoutBinding{5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, VK_NULL_HANDLE} ; //10 shadow buffer info -- per frame
    frameLayoutBindings[6] = VkDescriptorSetLayoutBinding{6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,  VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, VK_NULL_HANDLE} ;  //transform info -- per frame
    perFrameBindlessLayout = DescriptorSets::createVkDescriptorSetLayout(GetMainRendererContext(), frameLayoutBindings, "Per Frame Bindlses Layout");
    auto perFrameBindlessDescriptorData = DescriptorSets::CreateDescriptorDataForPipeline(GetMainRendererContext(), perFrameBindlessLayout,  true, frameLayoutBindings, "Per Frame Bindless Set", descriptorPool);


    perFrameDescriptorUpdates = MemoryArena::AllocSpan<std::span<DescriptorUpdateData>>(&rendererArena, MAX_FRAMES_IN_FLIGHT);
    for(int i = 0; i < MAX_FRAMES_IN_FLIGHT ; i++)
    {
        perFrameDescriptorUpdates[i] = CreatePerFrameDescriptorUpdates(i, glm::min(MAX_SHADOWCASTERS, shadowCasterCount), &rendererArena,  perFrameBindlessDescriptorData.layoutBindings);
    }
    VkDescriptorSetLayoutBinding copyBindings[3] = {};
    copyBindings[0] = VkDescriptorSetLayoutBinding{13, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1, VK_SHADER_STAGE_COMPUTE_BIT, VK_NULL_HANDLE}; //draws 
    copyBindings[1] = VkDescriptorSetLayoutBinding{14, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1, VK_SHADER_STAGE_COMPUTE_BIT, VK_NULL_HANDLE}; //ObjectData
    copyBindings[2] = VkDescriptorSetLayoutBinding{16, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1, VK_SHADER_STAGE_COMPUTE_BIT, VK_NULL_HANDLE}; //Early darw buffer
    
    VkDescriptorSetLayout copyLayout = DescriptorSets::createVkDescriptorSetLayout(GetMainRendererContext(), copyBindings, "Copy DraIndirect Layout");
    DescriptorDataForPipeline copyDescriptorData = DescriptorSets::CreateDescriptorDataForPipeline(GetMainRendererContext(), copyLayout, true, copyBindings, "Copy DraIndirect Set", descriptorPool);
   
    cullDataCopyLayoutIDX = pipelineLayoutManager.CreateNewGroup(GetMainRendererContext(), descriptorPool, {&copyDescriptorData, 1}, {&copyLayout, 1},  256, true, "Copy Draw layout");
    pipelineLayoutManager.CreatePipeline(cullDataCopyLayoutIDX, globalResources.shaderLoader->compiledShaders["pre_cull_copy"],  "pre_cull_copy",  {});
    
   VkDescriptorSetLayoutBinding cullLayoutBindings[7] = {};
	cullLayoutBindings[0] = VkDescriptorSetLayoutBinding{0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, MAX_SHADOWMAPS +1, VK_SHADER_STAGE_COMPUTE_BIT,  VK_NULL_HANDLE};// images 1 //per scene
	cullLayoutBindings[1] = VkDescriptorSetLayoutBinding{2, VK_DESCRIPTOR_TYPE_SAMPLER, MAX_SHADOWMAPS +1, VK_SHADER_STAGE_COMPUTE_BIT , VK_NULL_HANDLE} ;// iamges  4  // perscene
    cullLayoutBindings[2] = VkDescriptorSetLayoutBinding{12, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1, VK_SHADER_STAGE_COMPUTE_BIT, VK_NULL_HANDLE}; //frustum data
    cullLayoutBindings[3] = VkDescriptorSetLayoutBinding{13, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1, VK_SHADER_STAGE_COMPUTE_BIT, VK_NULL_HANDLE}; //draws 
    cullLayoutBindings[4] = VkDescriptorSetLayoutBinding{14, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1, VK_SHADER_STAGE_COMPUTE_BIT, VK_NULL_HANDLE}; //ObjectData
    cullLayoutBindings[5] = VkDescriptorSetLayoutBinding{15, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1, VK_SHADER_STAGE_COMPUTE_BIT, VK_NULL_HANDLE}; //ObjectData
    cullLayoutBindings[6] = VkDescriptorSetLayoutBinding{16, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1, VK_SHADER_STAGE_COMPUTE_BIT, VK_NULL_HANDLE}; //Early darw buffer
  
  
    VkDescriptorSetLayout _cullingLayout = DescriptorSets::createVkDescriptorSetLayout(GetMainRendererContext(), cullLayoutBindings, "Culling Layout");
    DescriptorDataForPipeline cullingDescriptorData = DescriptorSets::CreateDescriptorDataForPipeline(GetMainRendererContext(), _cullingLayout, true, cullLayoutBindings, "Culling Set", descriptorPool, 2);

    VkDescriptorSetLayoutBinding pyramidLayoutBindings[3] = {};
    pyramidLayoutBindings[0] = VkDescriptorSetLayoutBinding{12, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,1, VK_SHADER_STAGE_COMPUTE_BIT, VK_NULL_HANDLE}; //depth pyramid inout
    pyramidLayoutBindings[1] = VkDescriptorSetLayoutBinding{13, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,1, VK_SHADER_STAGE_COMPUTE_BIT, VK_NULL_HANDLE}; //depth pyramid output
    pyramidLayoutBindings[2] = VkDescriptorSetLayoutBinding{14, VK_DESCRIPTOR_TYPE_SAMPLER,1, VK_SHADER_STAGE_COMPUTE_BIT, VK_NULL_HANDLE}; //depth pyramid inout
    

  
   
    VkDescriptorSetLayout _mipchainLayout = DescriptorSets::createVkDescriptorSetLayout(GetMainRendererContext(), pyramidLayoutBindings, "MipChain Layout");
    DescriptorDataForPipeline mipChainDescriptorData = DescriptorSets::CreateDescriptorDataForPipeline(GetMainRendererContext(), _mipchainLayout, true, pyramidLayoutBindings, "MipChain Set", descriptorPool, HIZDEPTH * MAX_SHADOWMAPS + 16);
  

    mipChainLayoutIDX = pipelineLayoutManager.CreateNewGroup(GetMainRendererContext(), descriptorPool, {&mipChainDescriptorData, 1}, {&_mipchainLayout, 1},  sizeof(glm::vec2), true, "mip chain layout");
    cullingLayoutIDX = pipelineLayoutManager.CreateNewGroup(GetMainRendererContext(), descriptorPool, {&cullingDescriptorData, 1}, {&_cullingLayout, 1}, sizeof(GPU_CullPushConstants), true, "culling layout");
    pipelineLayoutManager.CreatePipeline(mipChainLayoutIDX, globalResources.shaderLoader->compiledShaders["mipChain"],  "mipChain",  {});
    pipelineLayoutManager.CreatePipeline(cullingLayoutIDX, globalResources.shaderLoader->compiledShaders["cull"],  "cull",  {});

    DescriptorDataForPipeline descriptorWrappers[2] = {perSceneBindlessDescriptorData, perFrameBindlessDescriptorData};
    VkDescriptorSetLayout layouts[2] = {perSceneBindlessDescriptorLayout, perFrameBindlessLayout};

    opaqueLayoutIDX = pipelineLayoutManager.CreateNewGroup(GetMainRendererContext(), descriptorPool, descriptorWrappers,layouts,  256, false, "opaque layout");
    shadowLayoutIDX = pipelineLayoutManager.CreateNewGroup(GetMainRendererContext(), descriptorPool, descriptorWrappers, layouts,  256, false, "shadow layout");



    //opaque
    auto swapchainFormat = vulkanObjects.swapchain.image_format;
    const GraphicsPipelineSettings opaquePipelineSettings =  {.colorFormats = std::span(&swapchainFormat, 1),.depthFormat =  globalResources.depthBufferInfoPerFrame[currentFrame].format, .depthWriteEnable = VK_TRUE};
    auto opaque1 = pipelineLayoutManager.CreatePipeline(opaqueLayoutIDX, globalResources.shaderLoader->compiledShaders["triangle_alt"],  "triangle_alt", opaquePipelineSettings);
    auto opaque2 = pipelineLayoutManager.CreatePipeline(opaqueLayoutIDX, globalResources.shaderLoader->compiledShaders["triangle"],  "triangle", opaquePipelineSettings);
    

    //shadow 
    const GraphicsPipelineSettings shadowPipelineSettings =  {std::span(&swapchainFormat, 0), shadowFormat, VK_CULL_MODE_NONE, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_TRUE, VK_TRUE, VK_TRUE, true };
    auto shadow = pipelineLayoutManager.CreatePipeline(shadowLayoutIDX, globalResources.shaderLoader->compiledShaders["shadow"],  "shadow", shadowPipelineSettings);
    const GraphicsPipelineSettings shadowDepthPipelineSettings =
        {std::span(&swapchainFormat, 0), shadowFormat, VK_CULL_MODE_BACK_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        VK_FALSE, VK_TRUE, VK_TRUE, true };

    const GraphicsPipelineSettings opaqueDepthPipelineSettings =
    {std::span(&swapchainFormat, 0), shadowFormat, VK_CULL_MODE_BACK_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    VK_FALSE, VK_TRUE, VK_FALSE, false };

    SHADOW_PREPASS_SHADER_INDEX = pipelineLayoutManager.CreatePipeline(shadowLayoutIDX, globalResources.shaderLoader->compiledShaders["shadow"],  "shadowDepthPrepass",  shadowDepthPipelineSettings);
    std::span<VkPipelineShaderStageCreateInfo> triangleShaderSpan =  globalResources.shaderLoader->compiledShaders["triangle"];
    OPAQUE_PREPASS_SHADER_INDEX = pipelineLayoutManager.CreatePipeline(opaqueLayoutIDX, triangleShaderSpan.subspan(0, 1),  "opaqueDepthPrepass",  opaqueDepthPipelineSettings);
    
    opaqueObjectShaderSets.push_back(opaque1, shadow);
    opaqueObjectShaderSets.push_back(opaque2, shadow);



    
    const GraphicsPipelineSettings linePipelineSettings =  {std::span(&swapchainFormat, 1), globalResources.depthBufferInfoPerFrame[currentFrame].format, VK_CULL_MODE_NONE, VK_PRIMITIVE_TOPOLOGY_LINE_LIST };
    DEBUG_LINE_SHADER_INDEX = pipelineLayoutManager.CreatePipeline(opaqueLayoutIDX, globalResources.shaderLoader->compiledShaders["lines"],  "lines", linePipelineSettings);

    
    const GraphicsPipelineSettings debugRaymarchPipelineSettings =  {std::span(&swapchainFormat, 1), globalResources.depthBufferInfoPerFrame[currentFrame].format, VK_CULL_MODE_FRONT_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};
    DEBUG_RAYMARCH_SHADER_INDEX = pipelineLayoutManager.CreatePipeline(opaqueLayoutIDX, globalResources.shaderLoader->compiledShaders["debug"],  "debug", debugRaymarchPipelineSettings);

    PlaceholderDepthPassLookup.Push( (shadow),  (SHADOW_PREPASS_SHADER_INDEX));
    PlaceholderDepthPassLookup.Push((opaque1), (OPAQUE_PREPASS_SHADER_INDEX));
    PlaceholderDepthPassLookup.Push((opaque2), (OPAQUE_PREPASS_SHADER_INDEX));
    PlaceholderDepthPassLookup.Push((DEBUG_RAYMARCH_SHADER_INDEX), (OPAQUE_PREPASS_SHADER_INDEX));
    PlaceholderDepthPassLookup.Push((DEBUG_LINE_SHADER_INDEX), (OPAQUE_PREPASS_SHADER_INDEX));

    //Cleanup
    for (auto kvp : globalResources.shaderLoader->compiledShaders)
    {
        for(auto compiled_shader : kvp.second)
        {
            vkDestroyShaderModule(vulkanObjects.vkbdevice.device, compiled_shader.module, nullptr);
        }
    }
}

void VulkanRenderer::InitializeRendererForScene(SceneSizeData sceneCountData) //todo remaining initialization refactor
{
    //shadows
    perLightShadowData = MemoryArena::AllocSpan<std::span<GPU_perShadowData>>(&rendererArena, sceneCountData.lightCount);
    auto scratchMemory = GetScratchStringMemory();
    for(int i = 0; i < MAX_FRAMES_IN_FLIGHT ; i++)
    {
        TextureUtilities::createImage(getPartialRendererContext(),SHADOW_MAP_SIZE, SHADOW_MAP_SIZE,shadowFormat,VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                                      VK_IMAGE_USAGE_SAMPLED_BIT,0,shadowResources.shadowImages[i],shadowResources.shadowMemory[i],1, MAX_SHADOWMAPS, true);
        TextureCreation::CreateTextureSampler(&shadowResources.shadowSamplers[i], GetMainRendererContext(), VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, 0, 1, true);
        fmt::format_to_n(scratchMemory.begin(),  scratchMemory.size(), "SHADOW IMAGE FRAME{}\0",i);
        SetDebugObjectName(vulkanObjects.vkbdevice.device, VK_OBJECT_TYPE_IMAGE, scratchMemory.subspan(0).data(), (uint64_t)(shadowResources.shadowImages[i]));
        fmt::format_to_n(scratchMemory.begin(),  scratchMemory.size(), "SHADOW SAMPLER FRAME{}\0",i);
        SetDebugObjectName(vulkanObjects.vkbdevice.device, VK_OBJECT_TYPE_SAMPLER, scratchMemory.subspan(0).data(), (uint64_t)(shadowResources.shadowSamplers[i]));
        UpdateShadowImageViews(i, sceneCountData);
    }

	//Textures the renderer globalls needs
    TextureCreation::CreateDepthPyramidSampler(&globalResources.writeDepthMipSampler, VK_SAMPLER_REDUCTION_MODE_MIN, GetMainRendererContext(), HIZDEPTH);
    TextureCreation::CreateDepthPyramidSampler(&globalResources.readDepthMipSampler, VK_SAMPLER_REDUCTION_MODE_MIN, GetMainRendererContext(), HIZDEPTH);
	
	//Uniform buffers for objects
	CreateUniformBuffers(sceneCountData.totalDrawCt, sceneCountData.objectCount, sceneCountData.lightCount);

	//Mesh data
	//TODO JS: Move -- should eventually run when meshes change
    PopulateMeshBuffers();


	//Initialize scene-ish objects we don't have a place for yet 
	//TODO: cubemaps should be driven from the scene, eventually
    TextureCreation::TextureImportRequest args[3] = {
    TextureCreation::MakeCreationArgsFromFilepathArgs("textures/outputLUT.png", &perFrameArenas[currentFrame], TextureType::LINEAR_DATA, VK_IMAGE_VIEW_TYPE_2D),
    TextureCreation::MakeTextureCreationArgsFromCachedKTX("textures/output_cubemap2_diff8.ktx2",VK_SAMPLER_ADDRESS_MODE_REPEAT, true),
    TextureCreation::MakeTextureCreationArgsFromCachedKTX("textures/output_cubemap2_spec8.ktx2",VK_SAMPLER_ADDRESS_MODE_REPEAT, true)};
    TextureData data[3];
    LoadTexturesThreaded(GetMainRendererContext(), data, args);
     auto cubemaplut_utilitytexture =data[0];
    cubemaplut_utilitytexture_index = AssetDataAndMemory->AddTexture(cubemaplut_utilitytexture);
     cube_irradiance =data[1];
     cube_specular = data[2];

}

//TODO JS: Support changing meshes at runtime
void VulkanRenderer::PopulateMeshBuffers()
{
    size_t indexCt = AssetDataAndMemory->meshData.vertIndices.size();
    auto Indices = MemoryArena::AllocSpan<uint32_t>(GetMainRendererContext().arena, indexCt);
    for(uint32_t j = 0; j <  AssetDataAndMemory->meshData.vertIndices.size(); j++)
    {
        Indices[j]  = static_cast<uint32_t>(AssetDataAndMemory->meshData.vertIndices[j]) ;
    }
     
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        GetFrameData(i).hostMesh.UpdateMappedMemory(AssetDataAndMemory->meshData.vertData.getSpan());
        GetFrameData(i).hostVerts.UpdateMappedMemory(AssetDataAndMemory->meshData.vertPositions.getSpan());
        GetFrameData(i).hostIndices.UpdateMappedMemory({Indices.data(),indexCt});
    }
}

//TODO JS: Better allocation strategy -- https://gpuopen-librariesandsdks.github.io/VulkanMemoryAllocator/html/usage_patterns.html
void VulkanRenderer::CreateUniformBuffers( size_t drawCount, size_t objectsCount,size_t lightCount)
{
    PerThreadRenderContext context = GetMainRendererContext();

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        GetFrameData(i).opaqueShaderGlobalsBuffer = CreateHostDataBuffer<GPU_ShaderGlobals>(&context, 1, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
        GetFrameData(i).perMeshbuffers = CreateHostDataBuffer<GPU_ObjectData>(&context, drawCount, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT); //
        GetFrameData(i).perObjectBuffers = CreateHostDataBuffer<GPU_Transform>(&context, objectsCount, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT); //
        GetFrameData(i).hostMesh = CreateHostDataBuffer<GPU_VertexData>(&context,AssetDataAndMemory->GetVertexCount(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
        GetFrameData(i).hostVerts = CreateHostDataBuffer<glm::vec4>(&context,AssetDataAndMemory->GetVertexCount(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT); //TODO JS: use index buffer, get vertex count

        GetFrameData(i).deviceVerts = { .data = VK_NULL_HANDLE, .size = (uint32_t)(AssetDataAndMemory->GetVertexCount() * sizeof(glm::vec4)), .mapped = nullptr };
        VmaAllocation alloc = {};
        BufferUtilities::CreateDeviceBuffer(context.allocator,"verts",
                                            GetFrameData(i).deviceVerts.size,
                                            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,context.device,&alloc,
                                            &GetFrameData(i).deviceVerts.data);

        GetFrameData(i).deviceMesh = { .data = VK_NULL_HANDLE, .size = (uint32_t)(AssetDataAndMemory->GetVertexCount() * sizeof(GPU_VertexData)), .mapped = nullptr };
        VmaAllocation alloc2 = {};
        BufferUtilities::CreateDeviceBuffer(context.allocator, "meshes",
                                            GetFrameData(i).deviceMesh.size,
                                            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,context.device,&alloc2,
                                            &GetFrameData(i).deviceMesh.data);
        
        GetFrameData(i).hostIndices = CreateHostDataBuffer<uint32_t>(&context,AssetDataAndMemory->GetIndexCount(), VK_BUFFER_USAGE_INDEX_BUFFER_BIT| VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
        GetFrameData(i).deviceIndices = {.data = VK_NULL_HANDLE, .size = (uint32_t)(AssetDataAndMemory->GetIndexCount() * sizeof(uint32_t)), .mapped = nullptr};
        VmaAllocation alloc3 = {};
        BufferUtilities::CreateDeviceBuffer(context.allocator, "indices",
                                            GetFrameData(i).deviceIndices.size,
                                            VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,context.device,&alloc3,
                                            &GetFrameData(i).deviceIndices.data);


        GetFrameData(i).lightBuffers = CreateHostDataBuffer<GPU_LightData>(&context,lightCount, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        GetFrameData(i).shadowDataBuffers = CreateHostDataBuffer<GPU_perShadowData>(&context,lightCount * 10, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
      
        
        GetFrameData(i).drawBuffers = CreateHostDataBuffer<drawCommandData>(&context, MAX_DRAWINDIRECT_COMMANDS, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
        SetDebugObjectNameS(context.device, VK_OBJECT_TYPE_BUFFER, "draw indirect buffer", (uint64_t)GetFrameData(i).drawBuffers.buffer.data);
        GetFrameData(i).frustumsForCullBuffers = CreateHostDataBuffer<glm::vec4>(&context, (MAX_SHADOWMAPS + MAX_CAMERAS) * 6, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

        GetFrameData(i).earlyDrawList = CreateHostDataBuffer<uint32_t>(&context, MAX_DRAWINDIRECT_COMMANDS, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
        SetDebugObjectNameS(context.device, VK_OBJECT_TYPE_BUFFER, "early draw buffer", (uint64_t)GetFrameData(i).earlyDrawList.buffer.data);
        auto earlyDrawSpan = GetFrameData(i).earlyDrawList.GetMappedView();
        for(size_t j = 0; j < earlyDrawSpan.size(); j++)
        {
            earlyDrawSpan[j] = ((uint32_t)1); 
        }

    }
}


#pragma region descriptorsets
void VulkanRenderer::CreateDescriptorSetPool(PerThreadRenderContext context, VkDescriptorPool* pool)
{
    
    std::vector<VkDescriptorPoolSize> sizes =
    {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 16 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 16 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 16 },
        { VK_DESCRIPTOR_TYPE_SAMPLER, 2048 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 2048 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2048 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2048 }
    };

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = 0;
    pool_info.poolSizeCount = (uint32_t)sizes.size();
    pool_info.pPoolSizes = sizes.data();
    
    pool_info.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT) * 1024;
    VK_CHECK( vkCreateDescriptorPool(context.device, &pool_info, nullptr, pool));
    context.threadDeletionQueue->push_backVk(deletionType::descriptorPool,uint64_t(*pool));
    
}



//Bindless descriptor updates
//DescriptorUpdateData contains the necesary data to update descriptor sets with DescriptorSets::UpdateDescriptorSet
//This creates a span for the bindless data which is set once per scene change 
std::span<DescriptorUpdateData> VulkanRenderer::CreatePerSceneDescriptorUpdates(uint32_t frame, MemoryArena::Allocator* arena, std::span<VkDescriptorSetLayoutBinding> layoutBindings)
{
    auto imageInfos = AssetDataAndMemory->textures.getSpan();
    std::span<VkDescriptorImageInfo> cubeImageInfos = MemoryArena::AllocSpan<VkDescriptorImageInfo>(arena, 2);
    cubeImageInfos[0] = cube_irradiance.vkImageInfo;
    cubeImageInfos[1] = cube_specular.vkImageInfo;
  
    VkDescriptorBufferInfo* vertBufferinfo = MemoryArena::Alloc<VkDescriptorBufferInfo>(arena); 
    *vertBufferinfo =GetFrameData(frame).deviceVerts.getBufferInfo();
    VkDescriptorBufferInfo* meshBufferinfo = MemoryArena::Alloc<VkDescriptorBufferInfo>(arena); 
    *meshBufferinfo =GetFrameData(frame).deviceMesh.getBufferInfo();


    Array<DescriptorUpdateData> descriptorUpdates = MemoryArena::AllocSpan<DescriptorUpdateData>(arena, layoutBindings.size());
    //Update descriptor sets with data
    descriptorUpdates.push_back({VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, imageInfos.data(),  (uint32_t)imageInfos.size()}); //scene
    descriptorUpdates.push_back({VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, cubeImageInfos.data(), (uint32_t)cubeImageInfos.size()}); //scene
    descriptorUpdates.push_back({VK_DESCRIPTOR_TYPE_SAMPLER, imageInfos.data(), (uint32_t)imageInfos.size()}); //scene 
    descriptorUpdates.push_back({VK_DESCRIPTOR_TYPE_SAMPLER, cubeImageInfos.data(), (uint32_t)cubeImageInfos.size()}); //scene
    descriptorUpdates.push_back({VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, meshBufferinfo}); //scene 
    descriptorUpdates.push_back({VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, vertBufferinfo}); //scene

    assert(descriptorUpdates.ct == layoutBindings.size());
    for(int i = 0; i < descriptorUpdates.ct; i++)
    {
        auto update = descriptorUpdates[i];
        auto layout = layoutBindings[i];
        assert(update.type == layout.descriptorType);
        assert(update.count <= layout.descriptorCount);
    }
 
   return descriptorUpdates.getSpan();
}

//Bindless descriptor updates
//DescriptorUpdateData contains the necesary data to update descriptor sets with DescriptorSets::UpdateDescriptorSet
//This creates a span for the bindless data which is set once per frame
std::span<DescriptorUpdateData> VulkanRenderer::CreatePerFrameDescriptorUpdates(uint32_t frame, size_t shadowCasterCount, MemoryArena::Allocator* arena, std::span<VkDescriptorSetLayoutBinding> layoutBindings)
{
    std::span<VkDescriptorImageInfo> shadows = MemoryArena::AllocSpan<VkDescriptorImageInfo>(arena, MAX_SHADOWMAPS);
    
    for(int i = 0; i < MAX_SHADOWMAPS; i++)
    {
        VkImageView view {};
        if (i < shadowCasterCount) 
        {
            view =  shadowResources.shadowMapTextureArrayViews[frame][i] ;
        }
        else
        {
            view = shadowResources.shadowMapTextureArrayViews[frame][0];
        }
        
        assert( &view != VK_NULL_HANDLE);
        VkDescriptorImageInfo shadowInfo = {
            .imageView = view, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };
        shadows[i] = shadowInfo;
    }
    VkDescriptorImageInfo* shadowSamplerInfo =  MemoryArena::Alloc<VkDescriptorImageInfo>(arena);
    *shadowSamplerInfo = {
        .sampler  = shadowResources.shadowSamplers[frame], .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };
    
    //frame
    VkDescriptorBufferInfo* perMeshBufferInfo = MemoryArena::Alloc<VkDescriptorBufferInfo>(arena); 
    *perMeshBufferInfo =GetFrameData(frame).perMeshbuffers.buffer.getBufferInfo();
    VkDescriptorBufferInfo* transformBufferInfo = MemoryArena::Alloc<VkDescriptorBufferInfo>(arena); 
    *transformBufferInfo =GetFrameData(frame).perObjectBuffers.buffer.getBufferInfo();
    VkDescriptorBufferInfo* shaderglobalsinfo =  MemoryArena::Alloc<VkDescriptorBufferInfo>(arena);
    *shaderglobalsinfo =GetFrameData(frame).opaqueShaderGlobalsBuffer.buffer.getBufferInfo();
    VkDescriptorBufferInfo* shadowBuffersInfo = MemoryArena::Alloc<VkDescriptorBufferInfo>(arena); 
    *shadowBuffersInfo =GetFrameData(frame).shadowDataBuffers.buffer.getBufferInfo();


    VkDescriptorBufferInfo* lightbufferinfo = MemoryArena::Alloc<VkDescriptorBufferInfo>(arena); 
    *lightbufferinfo =GetFrameData(frame).lightBuffers.buffer.getBufferInfo();
    VkDescriptorBufferInfo* meshBufferinfo = MemoryArena::Alloc<VkDescriptorBufferInfo>(arena); 
    *meshBufferinfo =GetFrameData(frame).deviceMesh.getBufferInfo();


    Array descriptorUpdates = MemoryArena::AllocSpan<DescriptorUpdateData>(arena, layoutBindings.size());
    //Update descriptor sets with data
    descriptorUpdates.push_back({VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, shaderglobalsinfo}); //frame
    descriptorUpdates.push_back({VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, shadows.data(),  MAX_SHADOWMAPS}); //shadows //scene
    descriptorUpdates.push_back({VK_DESCRIPTOR_TYPE_SAMPLER, shadowSamplerInfo, (uint32_t)1}); //shadows //scene

    descriptorUpdates.push_back({VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, lightbufferinfo}); //frame 
    descriptorUpdates.push_back({VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, perMeshBufferInfo}); //frame
    descriptorUpdates.push_back({VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, shadowBuffersInfo}); //frame 
    descriptorUpdates.push_back({VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, transformBufferInfo}); //frame


    assert(descriptorUpdates.ct == layoutBindings.size());
    for(int i = 0; i < descriptorUpdates.ct; i++)
    {
        auto update = descriptorUpdates[i];
        auto layout = layoutBindings[i];
        assert(update.type == layout.descriptorType);
        assert(update.count <= layout.descriptorCount);
    }
 
   return descriptorUpdates.getSpan();
}

//TODO optimize
uint32_t VulkanRenderer::CalculateTotalDrawCount(Scene* scene, std::span<PerSubmeshData> submeshData)
{
    uint32_t meshletCt = 0;
    for (auto& submeshIndex : scene->allSubmeshes.getSpan())
    {
        meshletCt += (uint32_t)submeshData[submeshIndex].meshletCt;
    }
    return meshletCt;
}

uint32_t VulkanRenderer::GetDrawCountForFrame(Scene* scene)
{

    return VulkanRenderer::CalculateTotalDrawCount(scene,  AssetDataAndMemory->meshData.perSubmeshData.getSpan());

}



#pragma region per-frame updates 

static void UpdateBindlessGlobals(cameraData camera, size_t lightCount, size_t cubeMapLutIndex, HostMappedDataBuffer<GPU_ShaderGlobals> globalsBuffer)
{
    GPU_ShaderGlobals globals{};
    viewProj vp = LightAndCameraHelpers::CalcViewProjFromCamera(camera);
    globals.viewMatrix = vp.view;
    globals.projMatrix = vp.proj;
    globals.eyePos = glm::vec4(camera.eyePos.x, camera.eyePos.y, camera.eyePos.z, 1);
    globals.lightcount_mode_shadowct_padding = glm::vec4(lightCount, debug_shader_bool_1, MAX_SHADOWCASTERS, 0);
    globals.lutIDX_lutSamplerIDX_padding_padding = glm::vec4(
        cubeMapLutIndex,
        cubeMapLutIndex, 0, 0);
    
    globalsBuffer.UpdateMappedMemory({&globals, 1});

}

static void UpdateShadowData(MemoryArena::Allocator* allocator, std::span<std::span<GPU_perShadowData>> perLightShadowData, Scene* scene, cameraData camera)
{
    for(int i =0; i <scene->lightCount; i++)
    {
        perLightShadowData[i] = LightAndCameraHelpers::CalculateLightMatrix(allocator, camera,
             (scene->lightposandradius[i]), scene->lightDir[i], scene->lightposandradius[i].w, static_cast<LightType>(scene->lightTypes[i]), &debugLinesManager);
    }
}


void VulkanRenderer::UpdatePerFrameBuffers(uint32_t currentFrame, Array<std::span<glm::mat4>> models, Scene* scene)
{
    auto tempArena = GetMainRendererContext().tempArena;

    UpdateBindlessGlobals(scene->sceneCamera, scene->lightCount, cubemaplut_utilitytexture_index, GetFrameData().opaqueShaderGlobalsBuffer);
    

    //Lights
    auto lights = MemoryArena::AllocSpan<GPU_LightData>(tempArena, scene->lightCount);
    std::span<GPU_perShadowData> flattenedPerShadowData = MemoryArena::AllocSpan<GPU_perShadowData>(&perFrameArenas[currentFrame],GetFrameData().shadowDataBuffers.Count());
    std::span<GPU_perShadowData> nextFlattenedShadowDataView = flattenedPerShadowData.subspan(0);
    size_t shadowOffset = 0;
    size_t spanOffset = 0;
    for (int i = 0; i < perLightShadowData.size(); i++)
    {
        size_t lightShadowMatrixCt = perLightShadowData[i].size();
        lights[i] = {
            scene->lightposandradius[i],
            scene->lightcolorAndIntensity[i],
            glm::vec4(scene->lightTypes[i], scene->lightDir[i].x, scene->lightDir[i].y, scene->lightDir[i].z),
            (uint32_t)shadowOffset,
            (uint32_t)lightShadowMatrixCt
        };

        std::copy(perLightShadowData[i].begin(), perLightShadowData[i].end(), nextFlattenedShadowDataView.begin());
        nextFlattenedShadowDataView = nextFlattenedShadowDataView.subspan(perLightShadowData[i].size());
        shadowOffset += lightShadowMatrixCt ;
    }
    
   GetFrameData().lightBuffers.UpdateMappedMemory(std::span(lights.data(), lights.size()));
   GetFrameData().shadowDataBuffers.UpdateMappedMemory(flattenedPerShadowData);

    // frustums
    auto frustums =GetFrameData().frustumsForCullBuffers
        .GetMappedView();

    int offset = 0;

    //TODO JS-frustum-binding: draws always need to be shadow first then opaque, because we bind frustum data like this
    for (int i = 0; i < perLightShadowData.size(); i++)
    {
        for (int j = 0; j < perLightShadowData[i].size(); j++)
        {
            glm::mat4  projT =   transpose(perLightShadowData[i][j].projMatrix)  ;
            frustums[offset + 0] = projT[3] + projT[0];
            frustums[offset + 1] = projT[3] - projT[0];
            frustums[offset + 2] = projT[3] + projT[1];
            frustums[offset + 3] = projT[3] - projT[1];
            frustums[offset + 4] = glm::vec4();
            frustums[offset + 5] = glm::vec4();
            offset +=6;
        }
    }
    //TODO JS-frustum-binding: draws always ned to be shadow first the nopaque because we bind frustum data like this
    viewProj vp = LightAndCameraHelpers::CalcViewProjFromCamera(scene->sceneCamera);
    glm::mat4  projT =   transpose(vp.proj);
    frustums[offset + 0] = projT[3] + projT[0];
    frustums[offset + 1] = projT[3] - projT[0];
    frustums[offset + 2] = projT[3] + projT[1];
    frustums[offset + 3] = projT[3] - projT[1];
    frustums[offset + 4] = glm::vec4();
    frustums[offset + 5] = glm::vec4();

    uint32_t drawCount = GetDrawCountForFrame(scene);
    //Ubos
    auto perDrawData = MemoryArena::AllocSpan<GPU_ObjectData>(tempArena,drawCount);
    auto transforms = MemoryArena::AllocSpan<GPU_Transform>(tempArena,scene->GetObjectsCount());

	GetFrameData().ObjectDataForFrame = perDrawData;
    GetFrameData().ObjectTransformsForFrame = transforms;
 
    uint32_t uboIndex = 0;
    for (uint32_t objectIndex = 0; objectIndex < scene->GetObjectsCount(); objectIndex++)
    {
        auto lookup = scene->transforms._transform_lookup[objectIndex];
        glm::mat4* model = &models[lookup.depth][lookup.index];
        auto subMeshCount = scene->objects.subMeshes[objectIndex].size();
        
        transforms[objectIndex].Model = *model;
        transforms[objectIndex].NormalMat = transpose(inverse(glm::mat3(*model)));

        //todo js MESHLET PERF: This is happening *per meshlet*, most (all except setting submesh index?) only needs to happen *per submesh*
        for(uint32_t subMeshIndex = 0; subMeshIndex < subMeshCount; subMeshIndex++)
        {
            uint32_t submeshIndex =scene->objects.subMeshes[objectIndex][subMeshIndex];
            auto& submeshdata = AssetDataAndMemory->meshData.perSubmeshData[submeshIndex];
            Material material = AssetDataAndMemory->materials[scene->objects.subMeshMaterials[objectIndex][subMeshIndex]];
            for(uint32_t meshletIndex = (uint32_t)submeshdata.firstMeshletIndex; meshletIndex <  (uint32_t)submeshdata.firstMeshletIndex + submeshdata.meshletCt; meshletIndex++)
            {
                perDrawData[uboIndex].props.indexInfo = glm::vec4(
                (material.diffuseIndex),
                    AssetDataAndMemory->GetOffsetFromMeshID(submeshIndex, meshletIndex),
                                               (999),
                                              objectIndex);

                perDrawData[uboIndex].props.textureIndexInfo = glm::vec4(material.diffuseIndex,
                    material.specIndex, material.normalIndex, -1.0);
                perDrawData[uboIndex].props.metallic = 0.0;
                perDrawData[uboIndex].props.roughness = material.roughness; 
                perDrawData[uboIndex].props.color = glm::vec4(material.color,1.0f);


        
                //Set position and radius for culling
                GPU_BoundingSphere meshSpacePositionAndRadius =
                    AssetDataAndMemory->meshData.boundingSpheres[meshletIndex];
                float meshRadius = meshSpacePositionAndRadius.radius;
                float objectScale = scene->transforms.worldUniformScales[lookup.depth][lookup.index];
                perDrawData[uboIndex].boundsSphere.center = meshSpacePositionAndRadius.center;
                perDrawData[uboIndex].objectScale = objectScale;
                // perDrawData[uboIndex].eyepos = glm::vec4(scene->sceneCamera.eyePos, 1);
                perDrawData[uboIndex].boundsSphere.radius = meshRadius;
                perDrawData[uboIndex].bounds =  AssetDataAndMemory->meshData.GPU_Boundses[meshletIndex];
                uboIndex++;
            }
        }
    }

    GetFrameData().perMeshbuffers.UpdateMappedMemory({perDrawData.data(), (size_t)drawCount});
    GetFrameData().perObjectBuffers.UpdateMappedMemory({transforms.data(), transforms.size()});



}

void EndCommandBufferRecording(ActiveRenderStepData* context)
{
    assert(context->commandBufferActive);
    VK_CHECK(vkEndCommandBuffer(context->commandBuffer));
    context->commandBufferActive = false;
}

VkRenderingAttachmentInfoKHR CreateRenderingAttatchmentWithLayout(VkImageView target, float clearColor, bool clear, VkImageLayout layout)
{
    VkClearDepthStencilValue depth = { .depth = 0, .stencil = 0 };
    VkClearColorValue ccv = { .float32 = {clearColor,clearColor,clearColor,1.0f} };
    VkClearValue clearvalue;
    clearvalue.color = ccv;
    clearvalue.depthStencil = depth; 
    return  {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
        .imageView = target,
        .imageLayout = layout,
        .loadOp = clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = clearvalue
    }; 
}
VkRenderingAttachmentInfoKHR CreateRenderingAttatchmentStruct(VkImageView target, float clearColor, bool clear)
{
   return CreateRenderingAttatchmentWithLayout(target, clearColor, clear,VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR);
}

void BeginRendering(VkRenderingAttachmentInfo* depthAttatchment, int colorAttatchmentCount, VkRenderingAttachmentInfo* colorAttatchment, VkExtent2D extent, VkCommandBuffer commandBuffer)
{
    VkRenderingInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;

    renderPassInfo.renderArea.extent = extent;
    renderPassInfo.renderArea.offset = {0, 0};
         
    renderPassInfo.layerCount =1;
    renderPassInfo.colorAttachmentCount = colorAttatchmentCount;
    renderPassInfo.pColorAttachments = colorAttatchment;
    renderPassInfo.pDepthAttachment = depthAttatchment;

    vkCmdBeginRendering(commandBuffer, &renderPassInfo);

    
	VkViewport viewport{
		.x = 0.0f,
		.y = 0.0f,
		.width = static_cast<float>(extent.width ),
		.height = static_cast<float>(extent.height),
		.minDepth = 0.0f,
		.maxDepth = 1.0f,
	};

    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    //
    
    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = extent;
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
}
void VulkanRenderer::UpdateDrawCommandCopyComputeBindings(ActiveRenderStepData commandBufferContext,
    ArenaAllocator arena)
{
     assert(commandBufferContext.commandBufferActive);
    VkDescriptorBufferInfo* computeDrawBuffer = MemoryArena::Alloc<VkDescriptorBufferInfo>(arena, 1); 
    *computeDrawBuffer = GetDescriptorBufferInfo(GetFrameData().drawBuffers);

    VkDescriptorBufferInfo* earlyDrawBuffer = MemoryArena::Alloc<VkDescriptorBufferInfo>(arena, 1); 
    *earlyDrawBuffer = GetDescriptorBufferInfo(GetFrameData().earlyDrawList);

    VkDescriptorBufferInfo* objectBufferInfo = MemoryArena::Alloc<VkDescriptorBufferInfo>(arena, 1); 
    *objectBufferInfo = GetDescriptorBufferInfo(GetFrameData().perMeshbuffers);
    
    std::span<DescriptorUpdateData> descriptorUpdates = MemoryArena::AllocSpan<DescriptorUpdateData>(arena, 3);

    descriptorUpdates[0] = {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, computeDrawBuffer}; //draws 
    descriptorUpdates[1] = {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, objectBufferInfo}; //ObjectData  //
    descriptorUpdates[2] = {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, earlyDrawBuffer}; //ObjectData  //

    auto descriptorData = pipelineLayoutManager.
        GetDescriptordata(cullDataCopyLayoutIDX, 0);
    auto& descriptorSetPool = descriptorData->descriptorSetsCaches[currentFrame];
    DescriptorSets::UpdateDescriptorSet(GetMainRendererContext(),descriptorSetPool.getNextDescriptorSet(),
     descriptorData->layoutBindings,
       descriptorUpdates); //Update desciptor sets for the compute bindings 

    pipelineLayoutManager.BindRequiredSetsToCommandBuffer(cullDataCopyLayoutIDX, commandBufferContext.commandBuffer, commandBufferContext.boundDescriptorSets, currentFrame, VK_PIPELINE_BIND_POINT_COMPUTE);

    vkCmdBindPipeline(commandBufferContext.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,   pipelineLayoutManager.GetPipeline(cullDataCopyLayoutIDX, 0));
}

void VulkanRenderer::UpdateComputeCullingBindings(ActiveRenderStepData commandBufferContext, Scene* scene,
    ArenaAllocator arena, bool latecull)
{

    std::span<VkDescriptorImageInfo> depthViews = MemoryArena::AllocSpan<VkDescriptorImageInfo>(arena, MAX_SHADOWMAPS + 1); //+1 for opaque
    std::span<VkDescriptorImageInfo> depthSampler = MemoryArena::AllocSpan<VkDescriptorImageInfo>(arena, MAX_SHADOWMAPS + 1);

        uint32_t cullingIndex = 0;

        int shadowMapIndex = 0;
        for(int i =0; i < scene->lightCount; i++)
        {
            for (int j = 0; j < shadowCountFromLightType(scene->lightTypes[i]); j++)
            {
                depthViews[cullingIndex] = {
                    .imageView = shadowResources.WIP_shadowDepthPyramidInfos[currentFrame][shadowMapIndex].view, 
                    .imageLayout = VK_IMAGE_LAYOUT_GENERAL 
                };
                depthSampler[cullingIndex] = {
                    .sampler  =globalResources.readDepthMipSampler, 
                    .imageLayout = VK_IMAGE_LAYOUT_GENERAL
                };
                shadowMapIndex++;
                cullingIndex++;
            }
        }
        depthViews[cullingIndex] = {
            .imageView = globalResources.depthPyramidInfoPerFrame[currentFrame].view, .imageLayout = VK_IMAGE_LAYOUT_GENERAL
        };
        depthSampler[cullingIndex] = {
            .sampler  =globalResources.readDepthMipSampler, .imageLayout = VK_IMAGE_LAYOUT_GENERAL
        };

        //Hack: fill the rest with padding
        for(uint32_t i = cullingIndex; i < MAX_SHADOWMAPS +1; i++)
        {
            depthViews[i] = {
                .imageView = globalResources.depthPyramidInfoPerFrame[currentFrame].view, .imageLayout = VK_IMAGE_LAYOUT_GENERAL
            };

        depthSampler[i] = {
            .sampler  =globalResources.readDepthMipSampler, .imageLayout = VK_IMAGE_LAYOUT_GENERAL
        };
    }
	

    assert(commandBufferContext.commandBufferActive);
    VkDescriptorBufferInfo* frustumData =  MemoryArena::Alloc<VkDescriptorBufferInfo>(arena, 1);
    *frustumData = GetDescriptorBufferInfo(GetFrameData().frustumsForCullBuffers);
    
    VkDescriptorBufferInfo* computeDrawBuffer = MemoryArena::Alloc<VkDescriptorBufferInfo>(arena, 1); 
    *computeDrawBuffer = GetDescriptorBufferInfo(GetFrameData().drawBuffers);

    VkDescriptorBufferInfo* earlyDrawBuffer = MemoryArena::Alloc<VkDescriptorBufferInfo>(arena, 1); 
    *earlyDrawBuffer = GetDescriptorBufferInfo(GetFrameData(latecull ?( currentFrame + 1) % MAX_FRAMES_IN_FLIGHT : currentFrame).earlyDrawList);

    VkDescriptorBufferInfo* objectBufferInfo = MemoryArena::Alloc<VkDescriptorBufferInfo>(arena, 1); 
    *objectBufferInfo = GetDescriptorBufferInfo(GetFrameData().perMeshbuffers);

    VkDescriptorBufferInfo* transformBufferInfo = MemoryArena::Alloc<VkDescriptorBufferInfo>(arena, 1); 
    *transformBufferInfo = GetDescriptorBufferInfo(GetFrameData().perObjectBuffers);
    
    std::span<DescriptorUpdateData> descriptorUpdates = MemoryArena::AllocSpan<DescriptorUpdateData>(arena, 7);

    descriptorUpdates[0] = {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, depthViews.data(),MAX_SHADOWMAPS +1}; 
    descriptorUpdates[1] = {VK_DESCRIPTOR_TYPE_SAMPLER, depthSampler.data(), MAX_SHADOWMAPS +1}; //draws 

    descriptorUpdates[2] = {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, frustumData};  //frustum data
    descriptorUpdates[3] = {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, computeDrawBuffer}; //draws 
    descriptorUpdates[4] = {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, objectBufferInfo}; //ObjectData  //
    descriptorUpdates[5] = {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, transformBufferInfo}; //ObjectData  //
    descriptorUpdates[6] = {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, earlyDrawBuffer}; //ObjectData  //

    auto cullingDescriptorData = pipelineLayoutManager.
        GetDescriptordata(cullingLayoutIDX, 0);
    auto& cullingLayoutSets = cullingDescriptorData->descriptorSetsCaches[currentFrame];
    DescriptorSets::UpdateDescriptorSet(GetMainRendererContext(),cullingLayoutSets.getNextDescriptorSet(),
     cullingDescriptorData->layoutBindings,
       descriptorUpdates); //Update desciptor sets for the compute bindings 

    pipelineLayoutManager.BindRequiredSetsToCommandBuffer(cullingLayoutIDX, commandBufferContext.commandBuffer, commandBufferContext.boundDescriptorSets, currentFrame, VK_PIPELINE_BIND_POINT_COMPUTE);

    vkCmdBindPipeline(commandBufferContext.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,   pipelineLayoutManager.GetPipeline(cullingLayoutIDX, 0));
}



#pragma endregion

#pragma region draw
void RecordDrawCommandCopyCompute(ArenaAllocator allocator, VkPipelineLayout layout, 
								  ActiveRenderStepData commandBufferContext, 
								  uint32_t offset, uint32_t count)
{
    struct GPU_CullCopyPushConstants
    {
        uint32_t drawOffset;
        uint32_t objectCount;
    };
    GPU_CullCopyPushConstants* pc = MemoryArena::Alloc<GPU_CullCopyPushConstants>(allocator);
    *pc = {
        .drawOffset = offset,
        .objectCount = count,
        };

    assert(commandBufferContext.commandBufferActive);

    auto& drawCount = count;

    vkCmdPushConstants(commandBufferContext.commandBuffer,  layout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                       sizeof(GPU_CullPushConstants), pc);

    const uint32_t dispatch_x = drawCount != 0
                                    ? 1 + static_cast<uint32_t>((drawCount - 1) / COPY_WORKGROUP_X)
                                    : 1;
    vkCmdDispatch(commandBufferContext.commandBuffer, dispatch_x, 1, 1);


}
void RecordCullingCompute(ArenaAllocator allocator, VkPipelineLayout layout, 
						  ActiveRenderStepData commandBufferContext,uint32_t passIndex, 
						  RenderPassDrawData conf, std::span<VkBufferMemoryBarrier2> barriers, bool lateCull)
{
    
    //Culling override code
    uint32_t cullFrustumIndex = passIndex * 6; 
    float nearPlane = conf.nearPlane;
    float farPlane = conf.farPlane;
    glm::mat4& proj = conf.proj;
    glm::mat4& view = conf.view;
    if (debug_cull_override)
    {
        cullFrustumIndex =  debug_cull_override_index * 6;
        passIndex =  debug_cull_override_index;
        nearPlane = DebugCullingData[passIndex].nearPlane;
        farPlane = DebugCullingData[passIndex].farPlane;
        proj = DebugCullingData[passIndex].proj;
        proj = DebugCullingData[passIndex].proj;
        
    }

    GPU_CullPushConstants* cullconstants = MemoryArena::Alloc<GPU_CullPushConstants>(allocator);
    *cullconstants = {
        .viewMatrix = conf.view,
        .projMatrix =  conf.proj,
        .drawOffset = conf.drawOffset,
		.passOffset =passIndex,
        .frustumOffset = cullFrustumIndex,
        .objectCount = conf.drawCount,
        .LATE_CULL = lateCull,
        .disable = (uint32_t)debug_shader_bool_2,
        .farPlane =  farPlane,
        .nearPlane = nearPlane
        };

    assert(commandBufferContext.commandBufferActive);

    auto& drawCount = conf.drawCount;

    vkCmdPushConstants(commandBufferContext.commandBuffer,  layout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                       sizeof(GPU_CullPushConstants), cullconstants);

    const uint32_t dispatch_x = drawCount != 0
                                    ? 1 + static_cast<uint32_t>((drawCount - 1) / CULL_WORKGROUP_X)
                                    : 1;
    vkCmdDispatch(commandBufferContext.commandBuffer, dispatch_x, 1, 1);
     


	SetPipelineBarrier(commandBufferContext.commandBuffer, barriers, {});

}

void VulkanRenderer::RecordMipChainCompute(ActiveRenderStepData commandBufferContext,
    ArenaAllocator arena, DepthPyramidInfo& pyramidInfo,
    VkImageView srcView
    )
{
    assert(commandBufferContext.commandBufferActive);
    vkCmdBindPipeline(commandBufferContext.commandBuffer,
        VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayoutManager.GetPipeline(mipChainLayoutIDX, 0));

    int i = 0;
   auto dstImage = pyramidInfo.image;
    auto pyramidviews = pyramidInfo.viewsForMips;
    auto pyramidWidth = pyramidInfo.depthSize.x;
    auto pyramidHeight = pyramidInfo.depthSize.y;
    
    //For each texture, for each mip level, we want to work on, dispatch X/Y texture resolution and set a barrier
	//(for the next phase)
    //Compute shader will read in texture and write back out to it
    VkImageMemoryBarrier2 barrier12 = GetImageBarrier(dstImage,
        VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
        VK_ACCESS_2_SHADER_READ_BIT |VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT ,
        VK_ACCESS_2_SHADER_WRITE_BIT   ,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_IMAGE_ASPECT_COLOR_BIT,
        i,
        HIZDEPTH);

    
	SetPipelineBarrier(commandBufferContext.commandBuffer, {}, { &barrier12, 1 });

   
    auto mipChainDescriptorConfig =  pipelineLayoutManager.GetDescriptordata(mipChainLayoutIDX, 0);
    auto mipChainDescriptorCache = &mipChainDescriptorConfig->descriptorSetsCaches[currentFrame];
    for(int i =0; i < HIZDEPTH; i++)
    {
        VkDescriptorImageInfo* sourceInfo =  MemoryArena::Alloc<VkDescriptorImageInfo>(arena);
        *sourceInfo = {
            .imageView = i == 0 ? srcView : pyramidviews[i-1], .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };

        VkDescriptorImageInfo* destinationInfo =  MemoryArena::Alloc<VkDescriptorImageInfo>(arena);
        *destinationInfo = {
            .imageView = pyramidviews[i], .imageLayout = VK_IMAGE_LAYOUT_GENERAL
        };

        VkDescriptorImageInfo* spyramidSamplerInfo =  MemoryArena::Alloc<VkDescriptorImageInfo>(arena);
        *spyramidSamplerInfo = {
            .sampler  =globalResources.writeDepthMipSampler, .imageLayout = VK_IMAGE_LAYOUT_GENERAL
        };

        
  
        std::span<DescriptorUpdateData> descriptorUpdates = MemoryArena::AllocSpan<DescriptorUpdateData>(arena, 3);

        descriptorUpdates[0] = {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, sourceInfo};  //src view
        descriptorUpdates[1] = {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, destinationInfo};  //dst view
        descriptorUpdates[2] = {VK_DESCRIPTOR_TYPE_SAMPLER, spyramidSamplerInfo}; //draws 

        //Memory barrier for compute access
        //Probably not ideal perf
        PipelineMemoryBarrier(commandBufferContext.commandBuffer,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR,
             VK_ACCESS_2_SHADER_WRITE_BIT_KHR,
             VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR,
             VK_ACCESS_2_SHADER_READ_BIT_KHR
              );
        
        DescriptorSets::UpdateDescriptorSet(GetMainRendererContext(),mipChainDescriptorCache->getNextDescriptorSet(), mipChainDescriptorConfig->layoutBindings,  descriptorUpdates); //Update desciptor sets for the compute bindings
        pipelineLayoutManager.BindRequiredSetsToCommandBuffer(mipChainLayoutIDX,commandBufferContext.commandBuffer, commandBufferContext.boundDescriptorSets, currentFrame, VK_PIPELINE_BIND_POINT_COMPUTE);
        
        uint32_t outputWidth= pyramidWidth >> i;
        uint32_t outputHeight = pyramidHeight >> i;
        auto size_x = outputWidth < 1 ? 1 : outputWidth;
        auto size_y = outputHeight < 1 ? 1 : outputHeight;
        uint32_t dispatchX =  1 + (size_x - 1) / MIP_WORKGROUP_X;
        uint32_t dispatchY =  1 + (size_y - 1) / MIP_WORKGROUP_Y;
        auto pushConstants = MemoryArena::Alloc<glm::vec2>(arena);
        *pushConstants = {size_x, size_y};
        vkCmdPushConstants(commandBufferContext.commandBuffer, pipelineLayoutManager.GetLayout(mipChainLayoutIDX),VK_SHADER_STAGE_COMPUTE_BIT, 0,
                          sizeof(glm::vec2),pushConstants);

        //Dispatch for all the pixels?
        vkCmdDispatch(commandBufferContext.commandBuffer, dispatchX, dispatchY, 1);

        VkImageMemoryBarrier2 barrier = GetImageBarrier(dstImage,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_MEMORY_WRITE_BIT,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
             VK_ACCESS_SHADER_READ_BIT,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_ASPECT_COLOR_BIT,
            i,
            1);

		SetPipelineBarrier(commandBufferContext.commandBuffer, {}, { &barrier, 1 });
    }

}
void VulkanRenderer::RecordUtilityPasses( VkCommandBuffer commandBuffer, size_t imageIndex)
{
    auto opaqueBindlessLayoutGroup =  pipelineLayoutManager.GetPipeline(DEBUG_LINE_SHADER_INDEX);
    VkPipelineLayout layout = pipelineLayoutManager.GetLayout(opaqueLayoutIDX);
    VkRenderingAttachmentInfoKHR dynamicRenderingColorAttatchment = CreateRenderingAttatchmentStruct(globalResources.swapchainImageViews[imageIndex], 0.0, false);
    VkRenderingAttachmentInfoKHR utilityDepthAttatchment =    CreateRenderingAttatchmentStruct( globalResources.depthBufferInfoPerFrame[currentFrame].view, 0.0, false);
    BeginRendering(
        &utilityDepthAttatchment,
        1, &dynamicRenderingColorAttatchment,
        vulkanObjects.swapchain.extent,commandBuffer);
    
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, opaqueBindlessLayoutGroup);
    // debugLines.push_back({{0,0,0},{20,40,20}, {0,0,1}});
    
    
    for (int i = 0; i <debugLinesManager.debugLines.size(); i++)
    {
    
        GPU_DebugLinePushConstants constants = debugLinesManager.getDebugLineForRendering(i);
        //Fullscreen quad render
        vkCmdPushConstants(commandBuffer, layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                          sizeof(GPU_DebugLinePushConstants), &constants);
        vkCmdDraw(commandBuffer, 2, 1, 0, 0);
    }


    //Debug raymarching pass
    //Disabled for now
    // vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, descriptorsetLayoutsData.getPipeline(debugRaymarchShaderIndex));
    //
    // vkCmdDraw(commandBuffer, 6, 1, 0, 0);
    //
    // //temp home for imgui
    // ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);

    
    vkCmdEndRendering(commandBuffer);


}

//This function maps all of our candidate draws to draw indirect buffers
//We end up with the gpu mapped DrawCommands populated with a unique list of drawcommanddata for each object for each draw
//I think I did it this way so I can easily set index count to zero later on to skip a draw
//Would be better to only have one list of objects to draw?
//(See DrawBatches.h)
void UpdateIndirectCommandBufferForPasses(Scene* scene, AssetManager* rendererData, MemoryArena::Allocator* allocator, 
										  std::span<drawCommandData> drawCommands, std::span<RenderBatch> passes)
{

    //indirect draw commands
    for(size_t i =0; i < passes.size(); i++)
    {
        uint32_t pipelinePassOffset = 0;
        auto _pass =passes[i];
        for (int j = 0; j < _pass.perPipelinePasses.size(); j++)
        {
            auto pass = _pass.perPipelinePasses[j];
            UpdateDrawCommandData(rendererData, 
                                                      drawCommands.subspan(
                                                          pass.offset.firstDrawIndex, pass.drawCount),
                                                      pass.sortedSubmeshes, pass.sortedfirstIndices);
            pipelinePassOffset += (uint32_t)pass.drawCount;
        }
    }

   
}

//Loop over RenderBatches (see DrawBatches.h) and submit draw commands.
//Lazily bind descriptor sets and switch pipelines as needed (batches are expected to already be sorted to minimize switches)
void SubmitRenderPassesForBatches( std::span<RenderBatch> Batches, VkBuffer indexBuffer, ActiveRenderStepData* renderStepContext, PipelineLayoutManager* pipelineLayoutManager, VkBuffer indirectCommandsBuffer,  int currentFrame)
{
   
    for(int j = 0; j < Batches.size(); j++)
    {
        auto passInfo = Batches[j];
        //Bind required resources for the pass
        assert(renderStepContext->commandBufferActive);
        pipelineLayoutManager->BindRequiredSetsToCommandBuffer(passInfo.pipelineLayoutGroup, renderStepContext->commandBuffer, renderStepContext->boundDescriptorSets, currentFrame);
        VkPipelineLayout layout = pipelineLayoutManager->GetLayout(passInfo.pipelineLayoutGroup);
        if (renderStepContext->boundIndexBuffer != indexBuffer )
        {
            vkCmdBindIndexBuffer(renderStepContext->commandBuffer, indexBuffer,0, VK_INDEX_TYPE_UINT32);
            renderStepContext->boundIndexBuffer = indexBuffer;
        }

        BeginRendering(
            passInfo.depthAttatchment,
            passInfo.colorattatchment == VK_NULL_HANDLE ? 0 : 1, passInfo.colorattatchment,
            passInfo.renderingAttatchmentExtent, renderStepContext->commandBuffer);
        
        if (passInfo.depthBiasSetting.use)
        {
            vkCmdSetDepthBias(
                        renderStepContext->commandBuffer,
                         passInfo.depthBiasSetting.depthBias,
                        0.0f,
                         passInfo.depthBiasSetting.slopeBias);
        }
        
        auto meshPasses = passInfo.perPipelinePasses;
        for(uint32_t i = 0; i < meshPasses.size(); i ++)
        {
            auto pipeline = pipelineLayoutManager->GetPipeline(meshPasses[i].shader);
            if (renderStepContext->boundPipeline != pipeline)
            {
                vkCmdBindPipeline(renderStepContext->commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline);
                renderStepContext->boundPipeline = pipeline;
            }

            if (passInfo.pushConstantsSize > 0)
            {
                vkCmdPushConstants(renderStepContext->commandBuffer, layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                                   passInfo.pushConstantsSize, passInfo.pushConstants);
            }
            
            uint32_t offset_into_struct = offsetof(drawCommandData, command);
            uint32_t offset_base =  meshPasses[i].offset.firstDrawIndex  *  sizeof(drawCommandData);
            uint32_t drawIndirectOffset = offset_base + offset_into_struct;
            vkCmdDrawIndexedIndirect(renderStepContext->commandBuffer,  indirectCommandsBuffer, drawIndirectOffset,  meshPasses[i].drawCount, sizeof(drawCommandData));
        }
        vkCmdEndRendering(renderStepContext->commandBuffer);
    }
}

void EndCommandBufferForStep(ActiveRenderStepData* stepData)
{
    assert(stepData->commandBufferActive);
    stepData->commandBufferActive = false;
    VK_CHECK(vkEndCommandBuffer(stepData->commandBuffer));
}

void SubmitCommandBuffer(ActiveRenderStepData* stepData)
{

    assert(!stepData->commandBufferActive);
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    VkPipelineStageFlags _waitStages= stepData->Queue == GET_QUEUES()->transferQueue ? VK_PIPELINE_STAGE_2_NONE : VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    submitInfo.waitSemaphoreCount = stepData->waitSemaphoreCt;
    submitInfo.pWaitSemaphores = stepData->waitSemaphore;
    submitInfo.pWaitDstStageMask =&_waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &stepData->commandBuffer;

    submitInfo.signalSemaphoreCount = stepData->signalSemaphoreCt;
    submitInfo.pSignalSemaphores = stepData->signalSempahore;
    
    //Submit pass 
    VK_CHECK(vkQueueSubmit(stepData->Queue, 1,
        &submitInfo, stepData->fence != nullptr? *stepData->fence : VK_NULL_HANDLE));
}


#pragma endregion



#pragma region perFrameUpdate

bool VulkanRenderer::hasStencilComponent(VkFormat format)
{
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

void VulkanRenderer::BeforeFirstUpdate()
{
    auto perSceneDescriptorData = pipelineLayoutManager.GetDescriptordata(opaqueLayoutIDX,0);
    //Update per scene descriptor sets (need to move)
    DescriptorSets::UpdateDescriptorSet(GetMainRendererContext(), perSceneDescriptorData->descriptorSetsCaches[0].getNextDescriptorSet(), perSceneDescriptorData->layoutBindings,perSceneDescriptorUpdates);
}
void VulkanRenderer::Update(Scene* scene)
{

    ////Imgui
    ImGui::Begin("RENDERER Buffer preview window");
    ImGui::Text("TODO");
    int imguiShadowIndex =1;
    VkDescriptorSet img = GetOrRegisterImguiTextureHandle(shadowResources.shadowSamplers[imguiShadowIndex],shadowResources.shadowMapTextureArrayViews[imguiShadowIndex][3]);
    //todo js: validation errors
    // VkDescriptorSet img_depth_0 = GetOrRegisterImguiTextureHandle(rendererResources.depthMipSampler,rendererResources.depthBufferInfo.view);
    // VkDescriptorSet img_depth_1 = GetOrRegisterImguiTextureHandle(rendererResources.depthMipSampler,rendererResources.depthPyramidInfo.viewsForMips[4]);
    //
    // ImGui::Image(ImTextureID(img_depth_0), ImVec2{480, 480});
    // ImGui::Image(ImTextureID(img_depth_1), ImVec2{480, 480});
    ImGui::End();
    ImGui::Render();


    //Wait for start of frame fences
    auto checkFences = haveInitializedFrame[currentFrame];
    if (checkFences)
    {
        clock_t before = clock();
        vkWaitForFences(vulkanObjects.vkbdevice.device, 1, &GetFrameData().inFlightFence, VK_TRUE, UINT64_MAX);
        clock_t afterFence = clock();
        clock_t difference = clock() - afterFence;
        auto msec = difference * 1000 / CLOCKS_PER_SEC;
        perFrameDeletionQueuse[currentFrame].FreeQueue();
        MemoryArena::Free(&perFrameArenas[currentFrame]);

    }

    //Pre frame 
    vkResetFences(vulkanObjects.vkbdevice.device, 1, &GetFrameData().inFlightFence);
    uint64_t RenderLoop =SDL_GetPerformanceCounter();
    pipelineLayoutManager.ResetForFrame(currentFrame);
    
//////Render the actual frame///////
    RenderFrame(scene);

    //Post frame
    debugLinesManager.debugLines.clear();
    auto t2 = SDL_GetPerformanceCounter();
    auto difference =SDL_GetPerformanceCounter() - RenderLoop;
    float msec = ((1000.0f * difference) / SDL_GetPerformanceFrequency());
    if (!checkFences)
    {
        haveInitializedFrame[currentFrame] = true;
        
    }
    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

VkPipelineStageFlags swapchainWaitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
VkPipelineStageFlags shadowWaitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

bool GetFlattenedShadowDataForIndex(int index, std::span<std::span<GPU_perShadowData>> inputShadowdata, GPU_perShadowData* result)
{

    int k =0;
    for( int i =0; i <= inputShadowdata.size(); i++)
    {
        for ( int j =0; j < inputShadowdata[i].size(); j++)
        {
            if (k == index)
            {
                *result = inputShadowdata[i][j];
               return true;
            }
            k++;
        }

    }
    return false;
}




struct passIndexInfo
{
    uint32_t submeshCt;
    uint32_t drawCt;
    uint32_t firstDraw;
};

//
RenderPassConfig CreateOpaquePassConfig(ArenaAllocator arena, RenderPassDrawData passInfo,
VkRenderingAttachmentInfoKHR* opaqueTarget,
VkRenderingAttachmentInfoKHR* depthTarget,
VkExtent2D targetExtent)
{
    auto resultConfigs = RenderPassConfig {

        .PushConstants = {nullptr, 0},
        .depthAttatchment = depthTarget,
        .colorAttatchment = opaqueTarget,
        .extents =targetExtent
        };
        
  
    return resultConfigs;

}

std::span<RenderPassConfig> CreateShadowPassConfigs(ArenaAllocator arena, std::span<RenderPassDrawData> passInfo,
                                     std::span<VkImageView> shadowMapRenderingViews)
{
    auto resultConfigs = MemoryArena::AllocSpan<RenderPassConfig>(arena, passInfo.size());
    for(size_t i = 0; i < passInfo.size(); i ++)
    {
            GPU_shadowPushConstant* shadowPushConstants = MemoryArena::Alloc<GPU_shadowPushConstant>(arena);
            shadowPushConstants->mat =   passInfo[i].proj * passInfo[i].view;
            VkRenderingAttachmentInfoKHR* depthDrawAttatchment =  MemoryArena::Alloc<VkRenderingAttachmentInfoKHR>(arena);
            *depthDrawAttatchment = CreateRenderingAttatchmentStruct(shadowMapRenderingViews[ i], 0.0, true);
            auto& config = resultConfigs[i];
            config.PushConstants = {(void*)shadowPushConstants, 256};
            config.depthAttatchment =depthDrawAttatchment;
            config.colorAttatchment =VK_NULL_HANDLE;
            config.extents = {SHADOW_MAP_SIZE, SHADOW_MAP_SIZE};
        
    }
    return resultConfigs.subspan(0, passInfo.size());

}


size_t UpdateDrawCommandData(AssetManager* rendererData,
                                                 std::span<drawCommandData> targetDrawCommandSpan,
                                                 std::span<uint32_t> submeshIndex, //Objects are drawn out of order compared to the underlying submesh buffer
                                                 std::span<uint32_t> submeshFirstDrawIndex //Draws are further out of order due to bucketing
                                                 )
{
    size_t drawCommandIndex = 0;
    
    for (size_t i = 0; i < submeshIndex.size(); i++)
    {
        auto subMeshIndex = submeshIndex[i];
        auto firstDraw = submeshFirstDrawIndex[i];
        for(uint32_t j =0; j < rendererData->meshData.perSubmeshData[subMeshIndex].meshletCt; j++)
        {
            auto meshletIndex = rendererData->meshData.perSubmeshData[subMeshIndex].firstMeshletIndex + j;
            targetDrawCommandSpan[drawCommandIndex++] = {
                // {},{}, {},
                (uint32_t)firstDraw + j,
                {
                    .indexCount = (uint32_t)rendererData->meshData.meshletInfo[meshletIndex].meshletIndexCount,
                    .instanceCount = 1,
                    .firstIndex = (uint32_t)rendererData->meshData.meshletInfo[meshletIndex].meshletIndexOffset,
                    .vertexOffset = (int32_t)rendererData->meshData.meshletInfo[meshletIndex].meshletVertexOffset ,
                    .firstInstance = firstDraw + j,
                }
            };
        }
    }
    return  targetDrawCommandSpan.size() ;
}


VkDescriptorSet UpdateAndGetBindlessDescriptorSetForFrame(PerThreadRenderContext context, DescriptorDataForPipeline* descriptorData, int currentFrame,  std::span<descriptorUpdates> updates)
{
    size_t idx = descriptorData->isPerFrame ? currentFrame : 0;
    auto descriptorSet = descriptorData->descriptorSetsCaches[idx].getNextDescriptorSet();
    DescriptorSets::UpdateDescriptorSet(context, descriptorSet, descriptorData->layoutBindings,updates[idx]);
    return descriptorSet;
}


uint32_t GetNextFirstIndex(RenderBatchQueue* q)
{
    if (q->batchConfigs.size() == 0 || q->batchConfigs.back().perPipelinePasses.size() == 0)
    {
        return 0;
    }
    auto& pass =  q->batchConfigs.back().perPipelinePasses.back();
    return pass.offset.firstDrawIndex + pass.drawCount;
}

VkCommandBuffer CreateAndBeginCommandBuffer(VkDevice device, CommandPoolManager* poolManager, bool transfer = false)
{
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = transfer? poolManager->transferCommandPool : poolManager->commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;
    
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer));
    //Begin command buffer
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT ; // Optional
    beginInfo.pInheritanceInfo = nullptr; // Optional
    
    VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));
    return commandBuffer;
}
void VulkanRenderer::RenderFrame(Scene* scene)
{
    superLuminalAdd("RenderFrame");
    vkResetFences(vulkanObjects.vkbdevice.device, 1, &GetFrameData().perFrameSemaphores.cullingFence);
    auto priorFrame = currentFrame  == 0? MAX_FRAMES_IN_FLIGHT-1 : currentFrame -1;
    auto nextFrame = (currentFrame  + 1) % MAX_FRAMES_IN_FLIGHT;
    if (debug_cull_override_index != internal_debug_cull_override_index)
    {
        debug_cull_override_index %= Scene::getShadowDataIndex(glm::min(scene->lightCount, MAX_SHADOWCASTERS), scene->lightTypes.getSpan());
        internal_debug_cull_override_index = debug_cull_override_index;
        printf("new cull index: %d! \n",  debug_cull_override_index);
    }
        
    //Update per frame data
    FrameData* thisFrameData = &GetFrameData();

    VkBufferMemoryBarrier2 indirectCommandsBarrier = GetBufferBarrier(thisFrameData->drawBuffers.buffer.data,
                                                                   VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                                                   VK_ACCESS_MEMORY_WRITE_BIT,
                                                                   VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT |
                                                                   VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
                                                                   VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT |
                                                                   VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT |
                                                                   VK_ACCESS_2_SHADER_READ_BIT |
                                                                   VK_ACCESS_2_SHADER_STORAGE_READ_BIT);
    VkBufferMemoryBarrier2 earlyDrawListBarrier = GetBufferBarrier(thisFrameData->earlyDrawList.buffer.data,
                                                                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                                                VK_ACCESS_MEMORY_WRITE_BIT,
                                                                VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT |
                                                                VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
                                                                VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT |
                                                                VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT |
                                                                VK_ACCESS_2_SHADER_READ_BIT |
                                                                VK_ACCESS_2_SHADER_STORAGE_READ_BIT);
    VkBufferMemoryBarrier2 earlyDrawListFirstBarrier = GetBufferBarrier(thisFrameData->earlyDrawList.buffer.data,
                                                              VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, //To synchronize with last frame
                                                              VK_ACCESS_MEMORY_WRITE_BIT,
                                                              VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT |
                                                              VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
                                                              VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT |
                                                              VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT |
                                                              VK_ACCESS_2_SHADER_READ_BIT |
                                                              VK_ACCESS_2_SHADER_STORAGE_READ_BIT);
    VkBufferMemoryBarrier2 indirectBarriers[2] = {indirectCommandsBarrier, earlyDrawListBarrier};

    VkImageMemoryBarrier2 shadowToOpaqueBarrier = GetImageBarrier(shadowResources.shadowImages[currentFrame],
                                                                  VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                                                  VK_ACCESS_2_SHADER_WRITE_BIT,
                                                                  VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                                                                  VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                                                                  VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                                                  VK_ACCESS_2_SHADER_WRITE_BIT |
                                                                  VK_ACCESS_2_SHADER_READ_BIT |
                                                                  VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                                                                  VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                                                                  VK_IMAGE_ASPECT_DEPTH_BIT,
                                                                  0,
                                                                  VK_REMAINING_MIP_LEVELS);

    auto afterShadowRenderBarriers = MemoryArena::AllocSpan<VkImageMemoryBarrier2>(&perFrameArenas[currentFrame], MAX_SHADOWMAPS);
    for (int i = 0; i < MAX_SHADOWMAPS; i++)
    {
        VkImageMemoryBarrier2 b = GetImageBarrier(shadowResources.shadowImages[currentFrame],
                                                                     VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                                                     VK_ACCESS_2_SHADER_WRITE_BIT,
                                                                     VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                                                                     VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                                                     VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                                                                     VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                                                                     VK_IMAGE_ASPECT_DEPTH_BIT ,
                                                                     0,
                                                                     1, i);
        afterShadowRenderBarriers[i] = b;
    }
    
	VkImageMemoryBarrier2 afterDepthDrawBarrier = GetImageBarrier(globalResources.depthBufferInfoPerFrame[currentFrame].image,
                                                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                                           VK_ACCESS_2_SHADER_WRITE_BIT,
                                                           VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                                                           VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                                           VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT,
                                                           VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                                                           VK_IMAGE_ASPECT_DEPTH_BIT,
                                                           0,
                                                           VK_REMAINING_MIP_LEVELS);

    VkImageMemoryBarrier2 depthtoCompute = GetImageBarrier(globalResources.depthBufferInfoPerFrame[currentFrame].image,
                                                           VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                                                           VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                                                           VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                                                           VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT |
                                                           VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
                                                           VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                                                           VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                                                           VK_IMAGE_ASPECT_DEPTH_BIT,
                                                           0,
                                                           VK_REMAINING_MIP_LEVELS);

    //barrier for shadows to compute
    VkImageMemoryBarrier2 shadowToCompute = GetImageBarrier(shadowResources.shadowImages[currentFrame],
                                                            VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                                                            VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                                                            VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                                                            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT |
                                                            VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_ACCESS_2_SHADER_SAMPLED_READ_BIT, 
                                                            VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT,
                                                            VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                                                            VK_IMAGE_ASPECT_DEPTH_BIT,
                                                            0,
                                                            VK_REMAINING_MIP_LEVELS);
    //Get swapchain image and depth attatchment
    VkRenderingAttachmentInfoKHR* depthDrawAttatchment = MemoryArena::Alloc<VkRenderingAttachmentInfoKHR>(&perFrameArenas[currentFrame]);
    *depthDrawAttatchment = CreateRenderingAttatchmentStruct( globalResources.depthBufferInfoPerFrame[currentFrame].view, 0.0, true);
    vkAcquireNextImageKHR(vulkanObjects.vkbdevice.device, vulkanObjects.swapchain,
        60000,thisFrameData->perFrameSemaphores.swapchainSemaphore, VK_NULL_HANDLE,&thisFrameData->swapChainIndex);
    //Sync data updated from the engine
    UpdateShadowData(&perFrameArenas[currentFrame], perLightShadowData, scene, scene->sceneCamera);
    //Update per frame descriptor sets
    UpdatePerFrameBuffers(currentFrame, scene->transforms.worldMatrices,scene); 
    UpdateAndGetBindlessDescriptorSetForFrame(GetMainRendererContext(), pipelineLayoutManager.GetDescriptordata(opaqueLayoutIDX, 1),currentFrame,perFrameDescriptorUpdates);

    

    //Kinda a convenience thing, todo more later, wrote in a hurry
    struct OrderedCommandBufferSteps
    {
        VkDevice device;
        Array<ActiveRenderStepData> renderstepDatas;
        ActiveRenderStepData* PushAndInitializeRenderStep(const char* cbufferDebugName, MemoryArena::Allocator* arena, CommandPoolManager* poolManager,
            VkSemaphore* WaitSemaphore = nullptr, 
             VkSemaphore* SignalSemaphore = nullptr, VkFence* CbufferSignalFence = nullptr, bool transferQueue = false) 
        {
            auto commandBuffer = CreateAndBeginCommandBuffer(device, poolManager, transferQueue);
            SetDebugObjectNameS(device,VK_OBJECT_TYPE_COMMAND_BUFFER, cbufferDebugName, (uint64_t)commandBuffer);
          
            auto boundDescriptorSets = MemoryArena::AllocSpan<VkDescriptorSet>(arena, 4);
            for (int i = 0; i <boundDescriptorSets.size(); i++)
            {
                boundDescriptorSets[i] = VK_NULL_HANDLE;
            }
            
            renderstepDatas.push_back({
                .commandBufferActive = true,
             .boundIndexBuffer = VK_NULL_HANDLE,
              .boundPipeline = VK_NULL_HANDLE,
                .commandBuffer = commandBuffer,
                 .Queue = transferQueue? GET_QUEUES()->transferQueue : GET_QUEUES()->graphicsQueue,
                  .waitSemaphore = WaitSemaphore,
                  .waitSemaphoreCt = (uint32_t)(WaitSemaphore == nullptr ? 0 : 1),
                   .signalSempahore = SignalSemaphore,
                   .signalSemaphoreCt = (uint32_t)(SignalSemaphore == nullptr ? 0 : 1),
                .boundDescriptorSets = boundDescriptorSets,
            .fence =  CbufferSignalFence});

            return &renderstepDatas.back();
        }


        
    };

    OrderedCommandBufferSteps renderSteps =
        {
        .device =   vulkanObjects.vkbdevice.device,
        .renderstepDatas =  MemoryArena::AllocSpan<ActiveRenderStepData>(&perFrameArenas[currentFrame], 20),
        };

    //I'm currently using multiple command buffers to handle most of my concurrency
    //each one of these step datas gets a signal and wait semaphore list, and then their command buffers are all submitted in order in the loop near the end of this fn
    //This needs revisted, but not right away
    //Create command buffers for this frame and allocate their signal semaphore/set up their dependencies. Written here in linear order

    /////////<Set up command buffers
    //////////
    /////////
    auto EarlyStep = renderSteps.PushAndInitializeRenderStep(
    "EarlyStep", &perFrameArenas[currentFrame],
    commandPoolmanager);
    
    auto BeforeCullingStep = renderSteps.PushAndInitializeRenderStep(
        "BeforeCullingStep", &perFrameArenas[currentFrame],
        commandPoolmanager, &thisFrameData->perFrameSemaphores.swapchainSemaphore, &thisFrameData->perFrameSemaphores.prepassSemaphore);
    
    auto CullingStep = renderSteps.PushAndInitializeRenderStep(
       "CullingStep", &perFrameArenas[currentFrame],
       commandPoolmanager, &thisFrameData->perFrameSemaphores.prepassSemaphore, nullptr,  &thisFrameData->perFrameSemaphores.cullingFence);

    auto OpaqueStep =renderSteps.PushAndInitializeRenderStep("OpaqueStep", &perFrameArenas[currentFrame],
        commandPoolmanager, nullptr, &thisFrameData->perFrameSemaphores.opaqueSemaphore);

    auto PostRenderStep =renderSteps.PushAndInitializeRenderStep("PostRenderStep", &perFrameArenas[currentFrame], //todo js: this shouldn't need to happen before presenting, and shouldnt need to signal the same fence
    commandPoolmanager, &thisFrameData->perFrameSemaphores.opaqueSemaphore, &thisFrameData->perFrameSemaphores.presentSemaphore,
    &GetFrameData().inFlightFence);

    VkImageMemoryBarrier2 swapChainTransitionInBarrier = GetImageBarrier(globalResources.swapchainImages[thisFrameData->swapChainIndex],
       VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, 
        VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
       VK_IMAGE_LAYOUT_UNDEFINED,
       VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT ,
        VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
       VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
       VK_IMAGE_ASPECT_COLOR_BIT,
       0,
       VK_REMAINING_ARRAY_LAYERS);

    
	SetPipelineBarrier(BeforeCullingStep->commandBuffer, {}, { &swapChainTransitionInBarrier, 1 });

    

    //Transferring cpu -> gpu data -- should improve
    AddBufferTrasnfer(thisFrameData->hostVerts.buffer.data, thisFrameData->deviceVerts.data,
                      thisFrameData->deviceVerts.size, EarlyStep->commandBuffer);
    AddBufferTrasnfer(thisFrameData->hostMesh.buffer.data, thisFrameData->deviceMesh.data,
                      thisFrameData->deviceMesh.size, EarlyStep->commandBuffer);
    AddBufferTrasnfer(thisFrameData->hostIndices.buffer.data, thisFrameData->deviceIndices.data,
                      thisFrameData->deviceIndices.size, EarlyStep->commandBuffer);

    //Set up draws
  
    //Create the renderpass
     auto* initialColorRenderTarget = MemoryArena::Alloc<VkRenderingAttachmentInfoKHR>(&perFrameArenas[currentFrame]);
     *initialColorRenderTarget =  CreateRenderingAttatchmentStruct(globalResources.swapchainImageViews[thisFrameData->swapChainIndex], 0.0, true);
    
    auto* ColorRenderTargetNoClear = MemoryArena::Alloc<VkRenderingAttachmentInfoKHR>(&perFrameArenas[currentFrame]);
    *ColorRenderTargetNoClear =  CreateRenderingAttatchmentStruct(globalResources.swapchainImageViews[thisFrameData->swapChainIndex], 0.0, false);

    //set up all our  draws
    

    CommonRenderPassData renderPassContext =
        {
        .tempAllocator =  &perFrameArenas[currentFrame],
        .scenePtr = scene,
        .assetDataPtr =  AssetDataAndMemory,
        };


    uint32_t drawOffset = 0;
    uint32_t submeshperPass =  (uint32_t)scene->objects.subMeshesCount;
    uint32_t drawPerPass = CalculateTotalDrawCount(scene,AssetDataAndMemory->meshData.perSubmeshData.getSpan());

    //Create the shadow data that shadow render passes will read from. This could happen anywhere in the frame
    Array flatShadowData = MemoryArena::AllocSpan<GPU_perShadowData>(&perFrameArenas[currentFrame], MAX_SHADOWMAPS);
    for(size_t i = 0; i < glm::min(scene->lightCount, MAX_SHADOWCASTERS); i ++)
    {
        LightType type = scene->lightTypes[i];
        flatShadowData.push_copy_span( perLightShadowData[i]);
    }

    //Create shadow info for passes
    Array shadowPassesData = MemoryArena::AllocSpan<RenderPassDrawData>(&perFrameArenas[currentFrame], MAX_RENDER_PASSES);
    uint32_t batchIndex =0;
    for (auto& shadowData : flatShadowData.getSpan())
    {
        shadowPassesData.push_back({.drawCount = drawPerPass,
            .drawOffset = drawOffset,
            .subMeshcount = submeshperPass,
            .proj = shadowData.projMatrix,
            .view = shadowData.viewMatrix,
            .farPlane = shadowData.farPlane,
            .nearPlane = shadowData.nearPlane});
        DebugCullingData[batchIndex++] = (shadowPassesData.back());
        drawOffset += drawPerPass;
    }
   
    //Culling debug stuff
    viewProj viewProjMatricesForCulling = LightAndCameraHelpers::CalcViewProjFromCamera(scene->sceneCamera);
    GPU_perShadowData* data = MemoryArena::Alloc<GPU_perShadowData>(&perFrameArenas[currentFrame]);
    if ( debug_cull_override && GetFlattenedShadowDataForIndex(debug_cull_override_index,perLightShadowData, data))
    {
        viewProjMatricesForCulling = { data->viewMatrix,  data->projMatrix};
        glm::vec4 frustumCornersWorldSpace[8] = {};
        LightAndCameraHelpers::FillFrustumCornersForSpace(frustumCornersWorldSpace,glm::inverse(viewProjMatricesForCulling.proj * viewProjMatricesForCulling.view));
        debugLinesManager.AddDebugFrustum(frustumCornersWorldSpace);
    }

    //Opaque info for pass
    RenderPassDrawData opaquePassData = {
        .drawCount = drawPerPass,
        .drawOffset = drawOffset,
        .subMeshcount = submeshperPass,
        .proj = viewProjMatricesForCulling.proj,
        .view = viewProjMatricesForCulling.view,
        .farPlane = 0.0,
        .nearPlane =  CAMERA_NEAR_PLANE };
    drawOffset += drawPerPass;
    DebugCullingData[batchIndex++] = (opaquePassData);
    
    Array<RenderBatch> renderBatches = MemoryArena::AllocSpan<RenderBatch>(&perFrameArenas[currentFrame], MAX_RENDER_PASSES);
    auto ShadowPassConfigs = CreateShadowPassConfigs(&perFrameArenas[currentFrame], shadowPassesData.getSpan(), shadowResources.shadowMapSingleLayerViews[currentFrame]);
    for (int i =0; i < ShadowPassConfigs.size(); i++)
    {
        renderBatches.push_back(CreateRenderBatch( &renderPassContext, ShadowPassConfigs[i],  shadowPassesData[i], true, shadowLayoutIDX, opaqueObjectShaderSets.shadowShaders.getSpan(), "shadow"));
    }
    
    //This "add batches" concept was an earlier misguided design choice. Right now it's still here, but I'm hackily getting subspans out of it so I cna submit them independantly in order, separated by barriers.
    auto shadowBatches = renderBatches.getSpan();

    auto opaqueconfig = CreateOpaquePassConfig(&perFrameArenas[currentFrame],opaquePassData,ColorRenderTargetNoClear, depthDrawAttatchment,  vulkanObjects.swapchain.extent);
    renderBatches.push_back(CreateRenderBatch(  &renderPassContext, opaqueconfig, opaquePassData,false,  opaqueLayoutIDX, opaqueObjectShaderSets.opaqueShaders.getSpan(), "opaque"));
    auto opaqueBatches  = renderBatches.getSpan().subspan(renderBatches.size() -1, 1);

    //Indirect command buffer
    UpdateIndirectCommandBufferForPasses(scene, AssetDataAndMemory, &perFrameArenas[currentFrame], thisFrameData->drawBuffers.GetMappedView(), renderBatches.getSpan());


    
 
    //Prototype depth passes code
    //Not sure I actually need depth prepasses? But can repurpose this for the early/late draws
    auto existingRenderBatches = renderBatches.getSpan();
    for(size_t i = 0; i < existingRenderBatches.size(); i++)
    {
        auto newBatch =  renderBatches[i];
        newBatch.colorattatchment = newBatch.colorattatchment  == ColorRenderTargetNoClear ?  initialColorRenderTarget : VK_NULL_HANDLE; //hack todo
        std::span<simpleMeshPassInfo> oldPasses = renderBatches[i].perPipelinePasses;
        std::span<simpleMeshPassInfo> newPasses = MemoryArena::AllocCopySpan<simpleMeshPassInfo>(&perFrameArenas[currentFrame], oldPasses);
        for(int j =0; j < newPasses.size(); j++)
        {
            // newPasses[j].shader = PlaceholderDepthPassLookup.Find(oldPasses[j].shader); 
        }
        newBatch.debugName = "depth prepass";
        newBatch.perPipelinePasses = newPasses;
        newBatch.depthAttatchment = MemoryArena::AllocCopy<VkRenderingAttachmentInfoKHR>(&perFrameArenas[currentFrame], *renderBatches[i].depthAttatchment);
        renderBatches.push_back(newBatch);
        renderBatches[i].depthAttatchment->loadOp = VK_ATTACHMENT_LOAD_OP_LOAD; 
    }
    
    auto prepassBatches = renderBatches.getSpan().subspan(existingRenderBatches.size());
    prepassBatches = MemoryArena::AllocCopySpan<RenderBatch>(&perFrameArenas[currentFrame], prepassBatches);

    if (haveInitializedFrame[currentFrame])
    {
        vkWaitForFences(vulkanObjects.vkbdevice.device, 1, &GetFrameData(priorFrame).perFrameSemaphores.cullingFence, VK_TRUE, UINT64_MAX);
        vkResetFences(vulkanObjects.vkbdevice.device, 1, &GetFrameData(priorFrame).perFrameSemaphores.cullingFence);
    }
    UpdateDrawCommandCopyComputeBindings(*BeforeCullingStep, &perFrameArenas[currentFrame]);

    
	SetPipelineBarrier(BeforeCullingStep->commandBuffer, { &earlyDrawListFirstBarrier, 1 }, {});
    //Pre-Cull copy (need to refactor, based on cull code atm)
    uint32_t copyOffset = 0;
    for(int j = 0; j < (shadowPassesData.size() < MAX_SHADOWMAPS_WITH_CULLING ? shadowBatches.size() : MAX_SHADOWMAPS_WITH_CULLING); j++)
    {

        RecordDrawCommandCopyCompute(&perFrameArenas[currentFrame],  pipelineLayoutManager.GetLayout(cullingLayoutIDX), *BeforeCullingStep,
        copyOffset, shadowPassesData[j].drawCount);
        copyOffset += shadowPassesData[j].drawCount;
    }

	RecordDrawCommandCopyCompute(&perFrameArenas[currentFrame],  pipelineLayoutManager.GetLayout(cullingLayoutIDX), *BeforeCullingStep,
						 copyOffset, opaquePassData.drawCount);
	copyOffset += opaquePassData.drawCount;


  

	SetPipelineBarrier(BeforeCullingStep->commandBuffer, indirectBarriers, {});
    
    UpdateComputeCullingBindings(*CullingStep,scene,  &perFrameArenas[currentFrame], false);
    //Submit the early prepass draws
    // auto& lastFrameCommands =  GetFrameData(priorFrame).drawBuffers.buffer.data;
    //Shadows (improve)
    SubmitRenderPassesForBatches(prepassBatches.subspan(0, prepassBatches.size() -1), thisFrameData->deviceIndices.data, BeforeCullingStep, &pipelineLayoutManager, thisFrameData->drawBuffers.buffer.data, currentFrame);
	SetPipelineBarrier(BeforeCullingStep->commandBuffer, {}, {&shadowToOpaqueBarrier, 1} );
    SubmitRenderPassesForBatches(prepassBatches.subspan(prepassBatches.size() -1), thisFrameData->deviceIndices.data, BeforeCullingStep, &pipelineLayoutManager, thisFrameData->drawBuffers.buffer.data, currentFrame);

   
    
    SetPipelineBarrier(BeforeCullingStep->commandBuffer, {}, {&depthtoCompute, 1} );

 
    SetPipelineBarrier(BeforeCullingStep->commandBuffer, {}, {&shadowToCompute, 1} );
    uint32_t shadowmapIndex = 0;
	//TODO: Use a more bindless structure for mipchain compute, bind resources once and index in to textures
    for(int i =0; i < scene->lightCount; i++)
    {

		for (int j = 0; j < shadowCountFromLightType(scene->lightTypes[i]); j++)
		{
		auto& shadowPyramidData = shadowResources.WIP_shadowDepthPyramidInfos[currentFrame][shadowmapIndex];
		RecordMipChainCompute(*BeforeCullingStep, &perFrameArenas[currentFrame], shadowPyramidData,
							shadowResources.shadowMapSingleLayerViews[currentFrame][shadowmapIndex]);
		shadowmapIndex++;
		}
    }
	DepthPyramidInfo& depthBufferPyramidData =   globalResources.depthPyramidInfoPerFrame[currentFrame];
    RecordMipChainCompute(*BeforeCullingStep, &perFrameArenas[currentFrame], depthBufferPyramidData,
						  globalResources.depthBufferInfoPerFrame[currentFrame].view);

 
    SetPipelineBarrier(OpaqueStep->commandBuffer, {}, afterShadowRenderBarriers );
    SetPipelineBarrier(OpaqueStep->commandBuffer, {}, {&afterDepthDrawBarrier, 1});

    //Culling for shadows
    uint32_t cullPassIndex = 0;
    for(int j = 0; j < (shadowBatches.size() < MAX_SHADOWMAPS_WITH_CULLING ? shadowBatches.size() : MAX_SHADOWMAPS_WITH_CULLING); j++)
    {
            RecordCullingCompute(&perFrameArenas[currentFrame],  pipelineLayoutManager.GetLayout(cullingLayoutIDX), *CullingStep,cullPassIndex++, shadowPassesData[j], indirectBarriers, false);
    }
    //Culling for Opaque
    for(int j = 0; j < opaqueBatches.size(); j++)
    {
        RecordCullingCompute(&perFrameArenas[currentFrame],  pipelineLayoutManager.GetLayout(cullingLayoutIDX), *CullingStep,cullPassIndex++, opaquePassData, indirectBarriers, false);
    }

    //shadows
    SubmitRenderPassesForBatches(shadowBatches,thisFrameData->deviceIndices.data, OpaqueStep, &pipelineLayoutManager, thisFrameData->drawBuffers.buffer.data, currentFrame);

    SetPipelineBarrier(OpaqueStep->commandBuffer, {}, {&shadowToOpaqueBarrier, 1} );
    //opaque
    SubmitRenderPassesForBatches(opaqueBatches,thisFrameData->deviceIndices.data, OpaqueStep, &pipelineLayoutManager, thisFrameData->drawBuffers.buffer.data, currentFrame);


  
    //After render steps
    //
    //
    RecordUtilityPasses(OpaqueStep->commandBuffer, currentFrame);
    VkImageMemoryBarrier2 swapchainToPresent = GetImageBarrier(globalResources.swapchainImages[thisFrameData->swapChainIndex],
         VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, 
         VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
         VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
         VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
         VK_ACCESS_MEMORY_READ_BIT,
         VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
         VK_IMAGE_ASPECT_COLOR_BIT,
         0,
         VK_REMAINING_MIP_LEVELS);

    //Drawing -> final mipchain sync
    SetPipelineBarrier(OpaqueStep->commandBuffer, {}, {&swapchainToPresent, 1} );
    SetPipelineBarrier(OpaqueStep->commandBuffer, {}, {&depthtoCompute, 1} );
	SetPipelineBarrier(OpaqueStep->commandBuffer, {}, { &shadowToCompute, 1 });
	SetPipelineBarrier(OpaqueStep->commandBuffer, indirectBarriers,{} );

    //Mip for late cull!
    uint32_t lateShadowMapIndex = 0;
    for(int i =0; i < scene->lightCount; i++)
    {

        for (int j = 0; j < shadowCountFromLightType(scene->lightTypes[i]); j++)
        {
            auto& shadowPyramidData = shadowResources.WIP_shadowDepthPyramidInfos[currentFrame][lateShadowMapIndex];
            RecordMipChainCompute(*PostRenderStep, &perFrameArenas[currentFrame], shadowPyramidData,
                                shadowResources.shadowMapSingleLayerViews[currentFrame][lateShadowMapIndex]);
            lateShadowMapIndex++;
        }
    }
    RecordMipChainCompute(*PostRenderStep, &perFrameArenas[currentFrame], depthBufferPyramidData,
                          globalResources.depthBufferInfoPerFrame[currentFrame].view);

    //Late cull!
    UpdateComputeCullingBindings(*PostRenderStep,scene,  &perFrameArenas[currentFrame], true);
    //Culling for shadows
    uint32_t lateCullPassIndex = 0;
    for(int j = 0; j < (shadowBatches.size() < MAX_SHADOWMAPS_WITH_CULLING ? shadowBatches.size() : MAX_SHADOWMAPS_WITH_CULLING); j++)
    {
        RecordCullingCompute(&perFrameArenas[currentFrame],  pipelineLayoutManager.GetLayout(cullingLayoutIDX), *PostRenderStep,
        lateCullPassIndex++, shadowPassesData[j],indirectBarriers, true);
    }
    //Culling for Opaque
    for(int j = 0; j < opaqueBatches.size(); j++)
    {
        RecordCullingCompute(&perFrameArenas[currentFrame],  pipelineLayoutManager.GetLayout(cullingLayoutIDX), *PostRenderStep,
        lateCullPassIndex++, opaquePassData,indirectBarriers, true);
    }

	SetPipelineBarrier(CullingStep->commandBuffer,indirectBarriers,{} );
    // //Submit commandbuffers
    EndCommandBufferForStep(EarlyStep);
    SubmitCommandBuffer(EarlyStep);
    
    EndCommandBufferForStep(BeforeCullingStep);
    SubmitCommandBuffer(BeforeCullingStep);
    
    EndCommandBufferForStep(CullingStep);
    SubmitCommandBuffer(CullingStep);

    EndCommandBufferForStep(OpaqueStep);
    SubmitCommandBuffer(OpaqueStep);

    EndCommandBufferForStep(PostRenderStep);
    SubmitCommandBuffer(PostRenderStep);

    
    //Present 
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount =  1;
    presentInfo.pWaitSemaphores = PostRenderStep->signalSempahore;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = {&vulkanObjects.swapchain.swapchain};
    presentInfo.pImageIndices = &GetFrameData().swapChainIndex;
    presentInfo.pResults = nullptr; // Optional

 
    vkQueuePresentKHR(GET_QUEUES()->presentQueue, &presentInfo);

 
    

    superLuminalEnd();
}

void VulkanRenderer::Cleanup()
{
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        vkWaitForFences(vulkanObjects.vkbdevice.device, 1, &GetFrameData().inFlightFence, VK_TRUE,
                        UINT64_MAX);
    }

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    deletionQueue->FreeQueue();
    pipelineLayoutManager.cleanup();
    AssetDataAndMemory->Cleanup(); //noop currently
    destroy_swapchain(vulkanObjects.swapchain);

    vkDestroySurfaceKHR(vulkanObjects.vkbInstance.instance, vulkanObjects.surface, nullptr);

    vkDeviceWaitIdle(vulkanObjects.vkbdevice.device);
    vmaDestroyAllocator(vulkanObjects.vmaAllocator); //TODO: Assert fails closing app -- may not be marking someting for deletion queue
    destroy_device(vulkanObjects.vkbdevice);
    destroy_instance(vulkanObjects.vkbInstance);
    SDL_DestroyWindow(_window);
}

#pragma endregion

void debug_PrintTransformGraph(localTransform g, int depth)
{
    for(int i = 0; i < depth; i++)
    {
        printf(".");
    }
    printf("%s \n ", g.name.c_str());
    if (!g.children.empty())
    {
        depth++;
        for(int i =0; i < g.children.size(); i++)
        {
            debug_PrintTransformGraph(*g.children[i], depth);
        }
    }
}

//Throw a bunch of VUID-vkCmdDrawIndexed-viewType-07752 errors due to image format, but renders fine
VkDescriptorSet VulkanRenderer::GetOrRegisterImguiTextureHandle(VkSampler sampler, VkImageView imageView)
{
        if (imguiRegisteredTextures.contains(imageView))
        {
            return imguiRegisteredTextures.at(imageView);
        }
  
     VkDescriptorSet set = ImGui_ImplVulkan_AddTexture(sampler, imageView,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
     imguiRegisteredTextures.emplace(imageView, set);
     return set;
    
}
void VulkanRenderer::initializeDearIMGUI()
{

    VkDescriptorPoolSize pool_sizes[] = { { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
    { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
    { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
    { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
    { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
    { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
    { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
    { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
    { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
    { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
    { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 } };

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000;
    pool_info.poolSizeCount = (uint32_t)std::size(pool_sizes);
    pool_info.pPoolSizes = pool_sizes;

    VK_CHECK(vkCreateDescriptorPool(vulkanObjects.vkbdevice.device, &pool_info, nullptr, &imgui_descriptorPool));
    deletionQueue->push_backVk(deletionType::descriptorPool, uint64_t(imgui_descriptorPool));

    ImGui_ImplSDL2_InitForVulkan(_window);
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = vulkanObjects.vkbInstance.instance;
    init_info.PhysicalDevice = vulkanObjects.vkbPhysicalDevice.physical_device;
    init_info.Device = vulkanObjects.vkbdevice.device;
    init_info.QueueFamily =GET_QUEUES()->graphicsQueueFamily;
    init_info.Queue =GET_QUEUES()->graphicsQueue;
    // init_info.PipelineCache = HOPEFULLY OPTIONAL??;
    init_info.DescriptorPool = imgui_descriptorPool;
    init_info.Subpass = 0;
    init_info.MinImageCount = 2;
    init_info.ImageCount = 2;
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.UseDynamicRendering = true;
    //dynamic rendering parameters for imgui to use
    
    init_info.PipelineRenderingCreateInfo = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
    init_info.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    init_info.PipelineRenderingCreateInfo.pColorAttachmentFormats = &vulkanObjects.swapchain.image_format;
    init_info.PipelineRenderingCreateInfo.depthAttachmentFormat = globalResources.depthBufferInfoPerFrame[currentFrame].format;
    //init_info.Allocator = nullptr; //
    //init_info.CheckVkResultFn = check_vk_result;
    ImGui_ImplVulkan_Init(&init_info);
    ImGui_ImplVulkan_CreateFontsTexture();
    // (this gets a bit more complicated, see example app for full reference)
    // ImGui_ImplVulkan_CreateFontsTexture(YOUR_COMMAND_BUFFER);
    // (your code submit a queue)
    //ImGui_ImplVulkan_DestroyFontUploadObjects(); //TODO JS: destroy queue
}

VulkanRenderer::FrameData& VulkanRenderer::GetFrameData(size_t frame)
{

    return perFrameData[frame];
    
}

VulkanRenderer::FrameData& VulkanRenderer::GetFrameData()
{

    return perFrameData[currentFrame];
    
}