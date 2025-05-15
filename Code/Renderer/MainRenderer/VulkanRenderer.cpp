#include "VulkanRenderer.h"
#undef main
#define SDL_MAIN_HANDLED 
#include <SDL2/SDL.h>
////
#include <cstdlib>
#include <functional>
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
#include <Renderer/MeshCreation/meshData.h>
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

struct gpuPerShadowData;
std::vector<unsigned int> pastTimes;
unsigned int averageCbTime;
unsigned int frames;
unsigned int MAX_TEXTURES = 120;
TextureData cube_irradiance;
TextureData cube_specular;
auto debugLinesManager = DebugLineData();
FullShaderHandle OPAQUE_PREPASS_SHADER_INDEX = {SIZE_MAX,SIZE_MAX};
FullShaderHandle SHADOW_PREPASS_SHADER_INDEX = {SIZE_MAX,SIZE_MAX};
FullShaderHandle DEBUG_LINE_SHADER_INDEX = {SIZE_MAX,SIZE_MAX};
FullShaderHandle  DEBUG_RAYMARCH_SHADER_INDEX ={SIZE_MAX,SIZE_MAX};
//Slowly making this less of a giant class that owns lots of things and moving to these static FNS -- will eventually migrate thewm


PerSceneShadowResources  init_allocate_shadow_memory(rendererObjects initializedrenderer,  MemoryArena::memoryArena* allocationArena);
VkDescriptorSet UpdateAndGetBindlessDescriptorSetForFrame(PerThreadRenderContext context, DescriptorDataForPipeline descriptorData, int currentFrame,  std::span<descriptorUpdates> updates);
size_t UpdateDrawCommanddataDrawIndirectCommands(std::span<drawCommandData> targetDrawCommandSpan, std::span<uint32_t> submeshIDstoSortedSubmeshIDs, std::span<uint32_t> submeshIDtoMeshID, std::span<uint32_t>meshIDtoFirstIndex, std::span<uint32_t> meshIDtoIndexCount);

VulkanRenderer::VulkanRenderer()
{
    this->deletionQueue = std::make_unique<RendererDeletionQueue>(rendererVulkanObjects.vkbdevice, rendererVulkanObjects.vmaAllocator);
  
    
    //Initialize memory arenas we use throughout renderer 
    this->rendererArena = {};
    MemoryArena::initialize(&rendererArena, 1000000 * 200); // 200mb
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        MemoryArena::initialize(&perFrameArenas[i], 100000 * 50);
        // 50mb each //TODO JS: Could be much smaller if I used stable memory for per frame verts and stuff
    }


    opaqueObjectShaderSets = BindlessObjectShaderGroup(&rendererArena, 10);
    
    //Initialize the window and vulkan renderer
    initializeWindow();
    rendererVulkanObjects = initializeVulkanObjects(_window, WIDTH, HEIGHT);

    //Initialize the main datastructures the renderer uses
    INIT_QUEUES(rendererVulkanObjects.vkbdevice);
    commandPoolmanager = std::make_unique<CommandPoolManager>(rendererVulkanObjects.vkbdevice, deletionQueue.get());
    globalResources = static_initializeResources(rendererVulkanObjects, &rendererArena, deletionQueue.get(),commandPoolmanager.get());
    shadowResources = init_allocate_shadow_memory(rendererVulkanObjects, &rendererArena);


    FramesInFlightData = MemoryArena::AllocSpan<per_frame_data>(&rendererArena, MAX_FRAMES_IN_FLIGHT);
    perFrameDeletionQueuse = MemoryArena::AllocSpanDefaultInitialize<std::unique_ptr<RendererDeletionQueue>>(&rendererArena, MAX_FRAMES_IN_FLIGHT); //todo js double create, oops
    //Command buffer stuff
    //semaphores
    for (int i = 0; i < FramesInFlightData.size(); i++)
    {
        createSemaphore(rendererVulkanObjects.vkbdevice.device, &(FramesInFlightData[i].semaphores.semaphore), "Per Frame semaphpre", deletionQueue.get());
        static_createFence(rendererVulkanObjects.vkbdevice.device,  &FramesInFlightData[i].inFlightFence, "Per Frame fence", deletionQueue.get());
       perFrameDeletionQueuse[i] = std::make_unique<RendererDeletionQueue>(rendererVulkanObjects.vkbdevice, rendererVulkanObjects.vmaAllocator); //todo js double create, oops
    }
    //Initialize sceneData
    AssetDataAndMemory = MemoryArena::Alloc<AssetManager>(&rendererArena);
    static_AllocateAssetMemory(&rendererArena, AssetDataAndMemory);
    //imgui
    initializeDearIMGUI();
    pastTimes.resize(9999);
}


PerThreadRenderContext VulkanRenderer::getMainRendererContext()
{
    return PerThreadRenderContext{
        .physicalDevice = rendererVulkanObjects.vkbdevice.physical_device,
        .device =  rendererVulkanObjects.vkbdevice.device,
        .textureCreationcommandPoolmanager = commandPoolmanager.get(),
        .allocator =  rendererVulkanObjects.vmaAllocator,
        .arena = &rendererArena,
        .tempArena = &perFrameArenas[currentFrame],
        .threadDeletionQueue = deletionQueue.get(),
        .ImportFence = VK_NULL_HANDLE, //todo js
        .vkbd = &rendererVulkanObjects.vkbdevice,
        };
}

BufferCreationContext VulkanRenderer::getPartialRendererContext()
{
    return objectCreationContextFromRendererContext(getMainRendererContext());
}



void VulkanRenderer::UpdateShadowImageViews(int frame, sceneCountData lightData)
{
    int i = frame;
       
    shadowResources.shadowMapRenderingImageViews[i] =  MemoryArena::AllocSpan<VkImageView>(&rendererArena, MAX_SHADOWMAPS);
    shadowResources.WIP_shadowDepthPyramidInfos[i] = MemoryArena::AllocSpan<DepthPyramidInfo>(&rendererArena, MAX_SHADOWMAPS);
    for (int j = 0; j < MAX_SHADOWMAPS; j++)
    {
        shadowResources.shadowMapRenderingImageViews[i][j] = TextureUtilities::createImageViewCustomMip(
            getPartialRendererContext(), shadowResources.shadowImages[i], shadowFormat,
            VK_IMAGE_ASPECT_DEPTH_BIT,
            (VkImageViewType)  VK_IMAGE_TYPE_2D,
            1, 
            j,1, 0);

        shadowResources.WIP_shadowDepthPyramidInfos[i][j] = static_createDepthPyramidResources(rendererVulkanObjects, &rendererArena, deletionQueue.get(), commandPoolmanager.get());
    }

    shadowResources.shadowMapSamplingImageViews[i] =  MemoryArena::AllocSpan<VkImageView>(&rendererArena, MAX_SHADOWCASTERS);
    for (size_t j = 0; j < MAX_SHADOWCASTERS; j++)
    {
        if (j < glm::min(lightData.lightCount, MAX_SHADOWCASTERS))
        {
            VkImageViewType type = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
            uint32_t ct = (uint32_t)shadowCountFromLightType(lightData.lightTypes[j]);
            if ((lightType)lightData.lightTypes[j] == LIGHT_POINT)
            {
                type = VK_IMAGE_VIEW_TYPE_CUBE;
            }
           
            shadowResources.shadowMapSamplingImageViews[i][j] = TextureUtilities::createImageViewCustomMip(
                getPartialRendererContext(), shadowResources.shadowImages[i], shadowFormat,
                VK_IMAGE_ASPECT_DEPTH_BIT,
                (VkImageViewType)  type,
                ct, 
                (uint32_t)Scene::getShadowDataIndex(j, lightData.lightTypes), 1, 0);

         
        }
        else
        {
            shadowResources.shadowMapSamplingImageViews[i][j] = shadowResources.shadowMapRenderingImageViews[i][j];
        }
    }
}


static PerSceneShadowResources init_allocate_shadow_memory(rendererObjects initializedrenderer,  MemoryArena::memoryArena* allocationArena)
{
  
    //TODO JS: it sucks that these aren't actually initialized in vulkan until later -- split them out? 
    auto shadowImages = MemoryArena::AllocSpan<VkImage>(allocationArena, MAX_FRAMES_IN_FLIGHT); 
    auto shadowSamplers = MemoryArena::AllocSpan<VkSampler>(allocationArena, MAX_FRAMES_IN_FLIGHT);
    auto shadowSamplersWithMips = MemoryArena::AllocSpan<VkSampler>(allocationArena, MAX_FRAMES_IN_FLIGHT);
    auto shadowMemory = MemoryArena::AllocSpan<VmaAllocation>(allocationArena, MAX_FRAMES_IN_FLIGHT);
    auto shadowMapRenderingImageViews = MemoryArena::AllocSpan<std::span<VkImageView>>(allocationArena, MAX_FRAMES_IN_FLIGHT); 
    auto shadowMapSamplingImageViews = MemoryArena::AllocSpan<std::span<VkImageView>>(allocationArena, MAX_FRAMES_IN_FLIGHT);
    auto shadowMapDepthImageInfos = MemoryArena::AllocSpan<std::span<DepthPyramidInfo>>(allocationArena, MAX_FRAMES_IN_FLIGHT); 

    return {
        
        .shadowMapRenderingImageViews = (shadowMapRenderingImageViews),
        .shadowMapSamplingImageViews =  (shadowMapSamplingImageViews),
        .shadowSamplers =shadowSamplers,
        .shadowImages =shadowImages,
        .WIP_shadowDepthPyramidInfos = shadowMapDepthImageInfos,
        .shadowMemory = shadowMemory,
    };
}



void VulkanRenderer::initializePipelines(size_t shadowCasterCount)
{
    
    createDescriptorSetPool(getMainRendererContext(), &descriptorPool);

    VkDescriptorSetLayout perSceneBindlessDescriptorLayout;
    VkDescriptorSetLayout perFrameBindlessLayout;
    
    VkDescriptorSetLayoutBinding sceneLayoutBindings[6] = {};
    sceneLayoutBindings[0] = VkDescriptorSetLayoutBinding{0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, MAX_TEXTURES, VK_SHADER_STAGE_FRAGMENT_BIT,  VK_NULL_HANDLE};// images 1 //per scene
    sceneLayoutBindings[1] = VkDescriptorSetLayoutBinding{1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,2, VK_SHADER_STAGE_FRAGMENT_BIT,  VK_NULL_HANDLE};//  cubes 2 //per scene?
    sceneLayoutBindings[2] = VkDescriptorSetLayoutBinding{2, VK_DESCRIPTOR_TYPE_SAMPLER, MAX_TEXTURES, VK_SHADER_STAGE_FRAGMENT_BIT , VK_NULL_HANDLE} ;// iamges  4  // perscene
    sceneLayoutBindings[3] = VkDescriptorSetLayoutBinding{3, VK_DESCRIPTOR_TYPE_SAMPLER, 2, VK_SHADER_STAGE_FRAGMENT_BIT, VK_NULL_HANDLE}  ;// iamges 5  // perscen
    sceneLayoutBindings[4] = VkDescriptorSetLayoutBinding{4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, VK_NULL_HANDLE} ; //Geometry//  // mesh 7 //per scene, for now
    sceneLayoutBindings[5] = VkDescriptorSetLayoutBinding{5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, VK_NULL_HANDLE} ;  //11 vert buffer info -- per scene, for now

    perSceneBindlessDescriptorLayout = DescriptorSets::createVkDescriptorSetLayout(getMainRendererContext(), sceneLayoutBindings, "Per Scene Bindlses Layout");
    auto perSceneBindlessDescriptorData = DescriptorSets::CreateDescriptorDataForPipeline(getMainRendererContext(), perSceneBindlessDescriptorLayout, false, sceneLayoutBindings, "Per Scene Bindless Set", descriptorPool);
    perSceneDescriptorUpdates = CreatePerSceneDescriptorUpdates(0, &rendererArena, perSceneBindlessDescriptorData.layoutBindings);

    VkDescriptorSetLayoutBinding frameLayoutBindings[7] = {};
    frameLayoutBindings[0] = VkDescriptorSetLayoutBinding{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL, VK_NULL_HANDLE}; //Globals  0 // per frame
    frameLayoutBindings[1] = VkDescriptorSetLayoutBinding{1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, MAX_SHADOWMAPS, VK_SHADER_STAGE_FRAGMENT_BIT  }; //SHADOW//  //shadow images 3 //per scene
    frameLayoutBindings[2] = VkDescriptorSetLayoutBinding{2, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_NULL_HANDLE}  ; // shadow iamges  6  // perscene
    frameLayoutBindings[3] = VkDescriptorSetLayoutBinding{3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,  VK_SHADER_STAGE_FRAGMENT_BIT, VK_NULL_HANDLE} ; //light//   //8 light info -- per frame
    frameLayoutBindings[4] = VkDescriptorSetLayoutBinding{4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,  VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, VK_NULL_HANDLE} ;  //9 Object info -- per frame
    frameLayoutBindings[5] = VkDescriptorSetLayoutBinding{5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, VK_NULL_HANDLE} ; //10 shadow buffer info -- per frame
    frameLayoutBindings[6] = VkDescriptorSetLayoutBinding{6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,  VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, VK_NULL_HANDLE} ;  //transform info -- per frame
    perFrameBindlessLayout = DescriptorSets::createVkDescriptorSetLayout(getMainRendererContext(), frameLayoutBindings, "Per Frame Bindlses Layout");
    auto perFrameBindlessDescriptorData = DescriptorSets::CreateDescriptorDataForPipeline(getMainRendererContext(), perFrameBindlessLayout,  true, frameLayoutBindings, "Per Frame Bindless Set", descriptorPool);


    perFrameDescriptorUpdates = MemoryArena::AllocSpan<std::span<descriptorUpdateData>>(&rendererArena, MAX_FRAMES_IN_FLIGHT);
    for(int i = 0; i < MAX_FRAMES_IN_FLIGHT ; i++)
    {
        perFrameDescriptorUpdates[i] = CreatePerFrameDescriptorUpdates(i, glm::min(MAX_SHADOWCASTERS, shadowCasterCount), &rendererArena,  perFrameBindlessDescriptorData.layoutBindings);
    }
   
   
   VkDescriptorSetLayoutBinding cullLayoutBindings[4] = {};
   cullLayoutBindings[0] = VkDescriptorSetLayoutBinding{12, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1, VK_SHADER_STAGE_COMPUTE_BIT, VK_NULL_HANDLE}; //frustum data
   cullLayoutBindings[1] = VkDescriptorSetLayoutBinding{13, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1, VK_SHADER_STAGE_COMPUTE_BIT, VK_NULL_HANDLE}; //draws 
   cullLayoutBindings[2] = VkDescriptorSetLayoutBinding{14, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1, VK_SHADER_STAGE_COMPUTE_BIT, VK_NULL_HANDLE}; //objectData
    cullLayoutBindings[3] = VkDescriptorSetLayoutBinding{15, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1, VK_SHADER_STAGE_COMPUTE_BIT, VK_NULL_HANDLE}; //objectData
  
  
    VkDescriptorSetLayout _cullingLayout = DescriptorSets::createVkDescriptorSetLayout(getMainRendererContext(), cullLayoutBindings, "Culling Layout");
    DescriptorDataForPipeline cullingDescriptorData = DescriptorSets::CreateDescriptorDataForPipeline(getMainRendererContext(), _cullingLayout, true, cullLayoutBindings, "Culling Set", descriptorPool);

    VkDescriptorSetLayoutBinding pyramidLayoutBindings[3] = {};
    pyramidLayoutBindings[0] = VkDescriptorSetLayoutBinding{12, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,1, VK_SHADER_STAGE_COMPUTE_BIT, VK_NULL_HANDLE}; //depth pyramid inout
    pyramidLayoutBindings[1] = VkDescriptorSetLayoutBinding{13, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,1, VK_SHADER_STAGE_COMPUTE_BIT, VK_NULL_HANDLE}; //depth pyramid output
    pyramidLayoutBindings[2] = VkDescriptorSetLayoutBinding{14, VK_DESCRIPTOR_TYPE_SAMPLER,1, VK_SHADER_STAGE_COMPUTE_BIT, VK_NULL_HANDLE}; //depth pyramid inout

  
   
    VkDescriptorSetLayout _mipchainLayout = DescriptorSets::createVkDescriptorSetLayout(getMainRendererContext(), pyramidLayoutBindings, "MipChain Layout");
    DescriptorDataForPipeline mipChainDescriptorData = DescriptorSets::CreateDescriptorDataForPipeline(getMainRendererContext(), _mipchainLayout, true, pyramidLayoutBindings, "MipChain Set", descriptorPool, HIZDEPTH);
  

    mipChainLayoutIDX = pipelineLayoutManager.CreateNewGroup(getMainRendererContext(), descriptorPool, {&mipChainDescriptorData, 1}, {&_mipchainLayout, 1},  sizeof(glm::vec2), true, "mip chain layout");
    cullingLayoutIDX = pipelineLayoutManager.CreateNewGroup(getMainRendererContext(), descriptorPool, {&cullingDescriptorData, 1}, {&_cullingLayout, 1}, sizeof(cullPConstants), true, "culling layout");
    pipelineLayoutManager.createPipeline(mipChainLayoutIDX, globalResources.shaderLoader->compiledShaders["mipChain"],  "mipChain",  {});
    pipelineLayoutManager.createPipeline(cullingLayoutIDX, globalResources.shaderLoader->compiledShaders["cull"],  "cull",  {});

    DescriptorDataForPipeline descriptorWrappers[2] = {perSceneBindlessDescriptorData, perFrameBindlessDescriptorData};
    VkDescriptorSetLayout layouts[2] = {perSceneBindlessDescriptorLayout, perFrameBindlessLayout};

    opaqueLayoutIDX = pipelineLayoutManager.CreateNewGroup(getMainRendererContext(), descriptorPool, descriptorWrappers,layouts,  256, false, "opaque layout");
    shadowLayoutIDX = pipelineLayoutManager.CreateNewGroup(getMainRendererContext(), descriptorPool, descriptorWrappers, layouts,  256, false, "shadow layout");



    //opaque
    auto swapchainFormat = rendererVulkanObjects.swapchain.image_format;
    const graphicsPipelineSettings opaquePipelineSettings =  {.colorFormats = std::span(&swapchainFormat, 1),.depthFormat =  globalResources.depthBufferInfoPerFrame[currentFrame].format, .depthWriteEnable = VK_FALSE};
    auto opaque1 = pipelineLayoutManager.createPipeline(opaqueLayoutIDX, globalResources.shaderLoader->compiledShaders["triangle_alt"],  "triangle_alt", opaquePipelineSettings);
    auto opaque2 = pipelineLayoutManager.createPipeline(opaqueLayoutIDX, globalResources.shaderLoader->compiledShaders["triangle"],  "triangle", opaquePipelineSettings);
    

    //shadow 
    const graphicsPipelineSettings shadowPipelineSettings =  {std::span(&swapchainFormat, 0), shadowFormat, VK_CULL_MODE_NONE, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_TRUE, VK_FALSE, VK_TRUE, true };
    auto shadow = pipelineLayoutManager.createPipeline(shadowLayoutIDX, globalResources.shaderLoader->compiledShaders["shadow"],  "shadow", shadowPipelineSettings);
    const graphicsPipelineSettings depthPipelineSettings =
        {std::span(&swapchainFormat, 0), shadowFormat, VK_CULL_MODE_NONE, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        VK_FALSE, VK_TRUE, VK_TRUE, true };

    const graphicsPipelineSettings depthPipelineSettings2 =
    {std::span(&swapchainFormat, 0), shadowFormat, VK_CULL_MODE_NONE, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    VK_FALSE, VK_TRUE, VK_FALSE, false };

    SHADOW_PREPASS_SHADER_INDEX = pipelineLayoutManager.createPipeline(shadowLayoutIDX, globalResources.shaderLoader->compiledShaders["shadow"],  "shadowDepthPrepass",  depthPipelineSettings);
    std::span<VkPipelineShaderStageCreateInfo> triangleShaderSpan =  globalResources.shaderLoader->compiledShaders["triangle"];
    OPAQUE_PREPASS_SHADER_INDEX = pipelineLayoutManager.createPipeline(opaqueLayoutIDX, triangleShaderSpan.subspan(0, 1),  "opaqueDepthPrepass",  depthPipelineSettings2);
    
    opaqueObjectShaderSets.push_back(opaque1, shadow,OPAQUE_PREPASS_SHADER_INDEX);
    opaqueObjectShaderSets.push_back(opaque2, shadow,OPAQUE_PREPASS_SHADER_INDEX);

    
  

    
    const graphicsPipelineSettings linePipelineSettings =  {std::span(&swapchainFormat, 1), globalResources.depthBufferInfoPerFrame[currentFrame].format, VK_CULL_MODE_NONE, VK_PRIMITIVE_TOPOLOGY_LINE_LIST };
    DEBUG_LINE_SHADER_INDEX = pipelineLayoutManager.createPipeline(opaqueLayoutIDX, globalResources.shaderLoader->compiledShaders["lines"],  "lines", linePipelineSettings);

    
    const graphicsPipelineSettings debugRaymarchPipelineSettings =  {std::span(&swapchainFormat, 1), globalResources.depthBufferInfoPerFrame[currentFrame].format, VK_CULL_MODE_FRONT_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};
    DEBUG_RAYMARCH_SHADER_INDEX = pipelineLayoutManager.createPipeline(opaqueLayoutIDX, globalResources.shaderLoader->compiledShaders["debug"],  "debug", debugRaymarchPipelineSettings);


    //Cleanup
    for (auto kvp : globalResources.shaderLoader->compiledShaders)
    {
        for(auto compiled_shader : kvp.second)
        {
            vkDestroyShaderModule(rendererVulkanObjects.vkbdevice.device, compiled_shader.module, nullptr);
        }
    }
}

//TODO JS: break dependency on Scene -- add some kind of interface or something.
//TODO JS: The layout creation stuff is awful -- need a full rethink of how layouts are set up and bound to. want a single centralized place.
void VulkanRenderer::InitializeRendererForScene(sceneCountData sceneCountData) //todo remaining initialization refactor
{
    //shadows
    perLightShadowData = MemoryArena::AllocSpan<std::span<PerShadowData>>(&rendererArena, sceneCountData.lightCount);
    
    for(int i = 0; i < MAX_FRAMES_IN_FLIGHT ; i++)
    {
        TextureUtilities::createImage(getPartialRendererContext(),SHADOW_MAP_SIZE, SHADOW_MAP_SIZE,shadowFormat,VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
               VK_IMAGE_USAGE_SAMPLED_BIT,0,shadowResources.shadowImages[i],shadowResources.shadowMemory[i],1, MAX_SHADOWMAPS, true);
        TextureCreation::createTextureSampler(&shadowResources.shadowSamplers[i], getMainRendererContext(), VK_SAMPLER_ADDRESS_MODE_REPEAT, 0, 1, true);
        setDebugObjectName(rendererVulkanObjects.vkbdevice.device, VK_OBJECT_TYPE_IMAGE, "SHADOW IMAGE", (uint64_t)(shadowResources.shadowImages[i]));
        setDebugObjectName(rendererVulkanObjects.vkbdevice.device, VK_OBJECT_TYPE_SAMPLER, "SHADOW IMAGE SAMPLER", (uint64_t)(shadowResources.shadowSamplers[i]));
        UpdateShadowImageViews(i, sceneCountData);
    }

    TextureCreation::createDepthPyramidSampler(&globalResources.depthMipSampler, getMainRendererContext(), HIZDEPTH);

    //Initialize scene-ish objects we don't have a place for yet 
     auto cubemaplut_utilitytexture = TextureCreation::CreateTextureSynchronously(
        getMainRendererContext(), TextureCreation::MakeCreationArgsFromFilepathArgs("textures/outputLUT.png", TextureType::DATA_DONT_COMPRESS));
    cubemaplut_utilitytexture_index = AssetDataAndMemory->AddTexture(cubemaplut_utilitytexture);
     cube_irradiance = TextureCreation::CreateTextureSynchronously(
        getMainRendererContext(), TextureCreation::MakeCreationArgsFromFilepathArgs("textures/output_cubemap2_diff8.ktx2",
            TextureType::CUBE));
     cube_specular = TextureCreation::CreateTextureSynchronously(
        getMainRendererContext(), TextureCreation::MakeCreationArgsFromFilepathArgs("textures/output_cubemap2_spec8.ktx2",
            TextureType::CUBE));

    CreateUniformBuffers(sceneCountData.subMeshCount, sceneCountData.objectCount, sceneCountData.lightCount);


    //TODO JS: Move... Run when meshes change?
    PopulateMeshBuffers();

    
}

//TODO JS: Support changing meshes at runtime
void VulkanRenderer::PopulateMeshBuffers()
{
    size_t indexCt = 0;
    size_t vertCt = 0;
    for (int i = 0; i < AssetDataAndMemory->meshCount; i++)
    {
        indexCt += AssetDataAndMemory->backing_meshes[i].indices.size();
        vertCt += AssetDataAndMemory->backing_meshes[i].vertices.size();
    }

    auto gpuVerts = MemoryArena::AllocSpan<gpuvertex>(getMainRendererContext().arena, vertCt);
    auto Positoins = MemoryArena::AllocSpan<glm::vec4>(getMainRendererContext().arena, vertCt);
    auto Indices = MemoryArena::AllocSpan<uint32_t>(getMainRendererContext().arena, indexCt);
    size_t vert = 0;
    size_t _vert = 0;
    uint32_t meshoffset = 0;
    for (int j = 0; j < AssetDataAndMemory->meshCount; j++)
    {
        MeshData mesh = AssetDataAndMemory->backing_meshes[j];
        for (int i = 0; i < mesh.indices.size(); i++)
        {
         
            Indices[vert++]  = mesh.indices[i] + meshoffset;
            // Positoins[vert] = pos;
         
        
        }
        for(int i =0; i < mesh.vertices.size(); i++)
        {

            glm::vec4 col = mesh.vertices[i].color;
            glm::vec4 uv = mesh.vertices[i].texCoord;
            glm::vec4 norm = mesh.vertices[i].normal;
            glm::vec4 tangent = mesh.vertices[i].tangent;

            gpuVerts[_vert] = {
                uv, norm, glm::vec4(tangent.x, tangent.y, tangent.z, tangent.w)
            };
            
            Positoins[_vert++] = mesh.vertices[i].pos;
        }
        meshoffset += static_cast<uint32_t>(mesh.vertices.size());
    }

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        FramesInFlightData[i].hostMesh.updateMappedMemory({gpuVerts.data(),vertCt});
        FramesInFlightData[i].hostVerts.updateMappedMemory({Positoins.data(),vertCt});
        FramesInFlightData[i].hostIndices.updateMappedMemory({Indices.data(), AssetDataAndMemory->getIndexCount()});
    }
}

//TODO JS: https://gpuopen-librariesandsdks.github.io/VulkanMemoryAllocator/html/usage_patterns.html advanced
void VulkanRenderer::CreateUniformBuffers( size_t submeshCount, size_t objectsCount,size_t lightCount)
{
    VkDeviceSize globalsSize = sizeof(ShaderGlobals);
    VkDeviceSize ubosSize = sizeof(gpu_per_draw) * submeshCount;
    VkDeviceSize vertsSize = sizeof(gpuvertex) * AssetDataAndMemory->getIndexCount();
    VkDeviceSize lightdataSize = sizeof(gpulight) *lightCount;
    VkDeviceSize shadowDataSize = sizeof(PerShadowData) *lightCount * 10; //times six is plenty right?

    PerThreadRenderContext context = getMainRendererContext();

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {

       

        
        FramesInFlightData[i].opaqueShaderGlobalsBuffer = createDataBuffer<ShaderGlobals>(&context, 1, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
        FramesInFlightData[i].perMeshbuffers = createDataBuffer<gpu_per_draw>(&context, submeshCount, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT); //
        FramesInFlightData[i].perObjectBuffers = createDataBuffer<gpu_transform>(&context, objectsCount, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT); //
        FramesInFlightData[i].hostMesh = createDataBuffer<gpuvertex>(&context,AssetDataAndMemory->getVertexCount(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
        FramesInFlightData[i].hostVerts = createDataBuffer<glm::vec4>(&context,AssetDataAndMemory->getVertexCount(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT); //TODO JS: use index buffer, get vertex count

        FramesInFlightData[i].deviceVerts = {.data = VK_NULL_HANDLE, .size = AssetDataAndMemory->getVertexCount() * sizeof(glm::vec4), .mapped = nullptr};
        VmaAllocation alloc = {};
        BufferUtilities::createDeviceBuffer(context.allocator,
                                            FramesInFlightData[i].deviceVerts.size,
                                            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,context.device,&alloc,
                                            &FramesInFlightData[i].deviceVerts.data);

        FramesInFlightData[i].deviceMesh = {.data = VK_NULL_HANDLE, .size = AssetDataAndMemory->getVertexCount() * sizeof(gpuvertex), .mapped = nullptr};
        VmaAllocation alloc2 = {};
        BufferUtilities::createDeviceBuffer(context.allocator,
                                            FramesInFlightData[i].deviceMesh.size,
                                            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,context.device,&alloc2,
                                            &FramesInFlightData[i].deviceMesh.data);
        
        FramesInFlightData[i].hostIndices = createDataBuffer<uint32_t>(&context,AssetDataAndMemory->getIndexCount(), VK_BUFFER_USAGE_INDEX_BUFFER_BIT| VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
        FramesInFlightData[i].deviceIndices = {.data = VK_NULL_HANDLE, .size = AssetDataAndMemory->getIndexCount() * sizeof(uint32_t), .mapped = nullptr};
        VmaAllocation alloc3 = {};
        BufferUtilities::createDeviceBuffer(context.allocator,
                                            FramesInFlightData[i].deviceIndices.size,
                                            VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,context.device,&alloc2,
                                            &FramesInFlightData[i].deviceIndices.data);


        FramesInFlightData[i].lightBuffers = createDataBuffer<gpulight>(&context,lightCount, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        FramesInFlightData[i].shadowDataBuffers = createDataBuffer<gpuPerShadowData>(&context,lightCount * 10, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
      
        
        FramesInFlightData[i].drawBuffers = createDataBuffer<drawCommandData>(&context, MAX_DRAWINDIRECT_COMMANDS, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
        setDebugObjectName(context.device, VK_OBJECT_TYPE_BUFFER, "draw indirect buffer", (uint64_t)FramesInFlightData[i].drawBuffers.buffer.data);
        FramesInFlightData[i].frustumsForCullBuffers = createDataBuffer<glm::vec4>(&context, (MAX_SHADOWMAPS + MAX_CAMERAS) * 6, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

    }
}


#pragma region descriptorsets
void VulkanRenderer::createDescriptorSetPool(PerThreadRenderContext context, VkDescriptorPool* pool)
{
    
    std::vector<VkDescriptorPoolSize> sizes =
    {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 80 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 80 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 80 },
        { VK_DESCRIPTOR_TYPE_SAMPLER, 80 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 80 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 80 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 80 }
    };

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = 0;
    pool_info.poolSizeCount = (uint32_t)sizes.size();
    pool_info.pPoolSizes = sizes.data();
    
    pool_info.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT) * 20 * 20;
    VK_CHECK( vkCreateDescriptorPool(context.device, &pool_info, nullptr, pool));
    context.threadDeletionQueue->push_backVk(deletionType::descriptorPool,uint64_t(*pool));
    
}




std::span<descriptorUpdateData> VulkanRenderer::CreatePerSceneDescriptorUpdates(uint32_t frame, MemoryArena::memoryArena* arena, std::span<VkDescriptorSetLayoutBinding> layoutBindings)
{
    //sceme
    auto imageInfos = AssetDataAndMemory->textures.getSpan();
    std::span<VkDescriptorImageInfo> cubeImageInfos = MemoryArena::AllocSpan<VkDescriptorImageInfo>(arena, 2);
    cubeImageInfos[0] = cube_irradiance.vkImageInfo;
    cubeImageInfos[1] = cube_specular.vkImageInfo;
  
    // *meshBufferinfo = FramesInFlightData[frame].meshBuffers.buffer.getBufferInfo();
    VkDescriptorBufferInfo* vertBufferinfo = MemoryArena::Alloc<VkDescriptorBufferInfo>(arena); 
    *vertBufferinfo = FramesInFlightData[frame].deviceVerts.getBufferInfo();
    VkDescriptorBufferInfo* meshBufferinfo = MemoryArena::Alloc<VkDescriptorBufferInfo>(arena); 
    *meshBufferinfo = FramesInFlightData[frame].deviceMesh.getBufferInfo();


    Array<descriptorUpdateData> descriptorUpdates = MemoryArena::AllocSpan<descriptorUpdateData>(arena, layoutBindings.size());
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

std::span<descriptorUpdateData> VulkanRenderer::CreatePerFrameDescriptorUpdates(uint32_t frame, size_t shadowCasterCount, MemoryArena::memoryArena* arena, std::span<VkDescriptorSetLayoutBinding> layoutBindings)
{
    std::span<VkDescriptorImageInfo> shadows = MemoryArena::AllocSpan<VkDescriptorImageInfo>(arena, MAX_SHADOWMAPS);
    
    for(int i = 0; i < MAX_SHADOWMAPS; i++)
    {
        VkImageView view {};
        if (i < shadowCasterCount)
        {
            view =  shadowResources.shadowMapSamplingImageViews[frame][i] ;
        }
        else
        {
            view = shadowResources.shadowMapRenderingImageViews[frame][0];
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
    *perMeshBufferInfo = FramesInFlightData[frame].perMeshbuffers.buffer.getBufferInfo();
    VkDescriptorBufferInfo* transformBufferInfo = MemoryArena::Alloc<VkDescriptorBufferInfo>(arena); 
    *transformBufferInfo = FramesInFlightData[frame].perObjectBuffers.buffer.getBufferInfo();
    VkDescriptorBufferInfo* shaderglobalsinfo =  MemoryArena::Alloc<VkDescriptorBufferInfo>(arena);
    *shaderglobalsinfo = FramesInFlightData[frame].opaqueShaderGlobalsBuffer.buffer.getBufferInfo();
    VkDescriptorBufferInfo* shadowBuffersInfo = MemoryArena::Alloc<VkDescriptorBufferInfo>(arena); 
    *shadowBuffersInfo = FramesInFlightData[frame].shadowDataBuffers.buffer.getBufferInfo();


    // *meshBufferinfo = FramesInFlightData[frame].meshBuffers.buffer.getBufferInfo();
    VkDescriptorBufferInfo* lightbufferinfo = MemoryArena::Alloc<VkDescriptorBufferInfo>(arena); 
    *lightbufferinfo = FramesInFlightData[frame].lightBuffers.buffer.getBufferInfo();
    VkDescriptorBufferInfo* meshBufferinfo = MemoryArena::Alloc<VkDescriptorBufferInfo>(arena); 
    *meshBufferinfo = FramesInFlightData[frame].deviceMesh.getBufferInfo();


    Array descriptorUpdates = MemoryArena::AllocSpan<descriptorUpdateData>(arena, layoutBindings.size());
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

//TODO JS: none of this belongs here except for actually submitting the updates
#pragma region per-frame updates 



static void updateGlobals(cameraData camera, size_t lightCount, size_t cubeMapLutIndex, HostDataBufferObject<ShaderGlobals> globalsBuffer)
{
    ShaderGlobals globals{};
    viewProj vp = LightAndCameraHelpers::CalcViewProjFromCamera(camera);
    globals.view = vp.view;
    globals.proj = vp.proj;
    globals.viewPos = glm::vec4(camera.eyePos.x, camera.eyePos.y, camera.eyePos.z, 1);
    globals.lightcountx_modey_shadowcountz_padding_w = glm::vec4(lightCount, debug_shader_bool_1, MAX_SHADOWCASTERS, 0);
    globals.cubemaplutidx_cubemaplutsampleridx_paddingzw = glm::vec4(
        cubeMapLutIndex,
        cubeMapLutIndex, 0, 0);
    
    globalsBuffer.updateMappedMemory({&globals, 1});

}

static void updateShadowData(MemoryArena::memoryArena* allocator, std::span<std::span<PerShadowData>> perLightShadowData, Scene* scene, cameraData camera)
{
    for(int i =0; i <scene->lightCount; i++)
    {
        perLightShadowData[i] = LightAndCameraHelpers::CalculateLightMatrix(allocator, camera,
             (scene->lightposandradius[i]), scene->lightDir[i], scene->lightposandradius[i].w, static_cast<lightType>(scene->lightTypes[i]), &debugLinesManager);
    }
}

//from niagara
static glm::vec4 normalizePlane(glm::vec4 p)
{
    return p / length(glm::vec3(p));
}

void VulkanRenderer::updatePerFrameBuffers(uint32_t currentFrame, Array<std::span<glm::mat4>> models, Scene* scene)
{
    //TODO JS: to ring buffer?
    auto tempArena = getMainRendererContext().tempArena;

    updateGlobals(scene->sceneCamera, scene->lightCount, cubemaplut_utilitytexture_index, FramesInFlightData[currentFrame].opaqueShaderGlobalsBuffer);
    

    //Lights
    auto lights = MemoryArena::AllocSpan<gpulight>(tempArena, scene->lightCount);
    Array<gpuPerShadowData> flattenedPerShadowData = Array(MemoryArena::AllocSpan<gpuPerShadowData>(&perFrameArenas[currentFrame],FramesInFlightData[currentFrame].shadowDataBuffers.count()));
    for (int i = 0; i < perLightShadowData.size(); i++)
    {
        std::span<gpuPerShadowData> lightsShadowData  =  MemoryArena::AllocSpan<gpuPerShadowData>(tempArena, perLightShadowData[i].size());
        for (int j = 0; j < perLightShadowData[i].size(); j++)
        {
            lightsShadowData[j] = { perLightShadowData[i][j].view,perLightShadowData[i][j].proj, perLightShadowData[i][j].cascadeDepth};
        }
        lights[i] = {
            scene->lightposandradius[i],
            scene->lightcolorAndIntensity[i],
            glm::vec4(scene->lightTypes[i], scene->lightDir[i].x, scene->lightDir[i].y, scene->lightDir[i].z),
            glm::vec4(flattenedPerShadowData.ct, lightsShadowData.size(), -1, -1)
        };

        for (int j = 0; j < lightsShadowData.size(); j++)
        {
            flattenedPerShadowData.push_back(lightsShadowData[j]);
        }
    }
    
    FramesInFlightData[currentFrame].lightBuffers.updateMappedMemory(std::span(lights.data(), lights.size()));
    FramesInFlightData[currentFrame].shadowDataBuffers.updateMappedMemory(std::span(flattenedPerShadowData.data, flattenedPerShadowData.capacity));

    // frustums
    auto frustums = FramesInFlightData[currentFrame].frustumsForCullBuffers
        .getMappedSpan();

    int offset = 0;
    //TODO JS-frustum-binding: draws always ned to be shadow first the nopaque because we bind frustum data like this
    for (int i = 0; i < perLightShadowData.size(); i++)
    {
        for (int j = 0; j < perLightShadowData[i].size(); j++)
        {
            viewProj vp = LightAndCameraHelpers::CalcViewProjFromCamera(scene->sceneCamera);
            glm::mat4  projT =   transpose(perLightShadowData[i][j].proj)  ;
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
    

    //Ubos
    auto perMesh = MemoryArena::AllocSpan<gpu_per_draw>(tempArena,scene->objects.subMeshesCount);
    auto transforms = MemoryArena::AllocSpan<gpu_transform>(tempArena,scene->ObjectsCount());

    uint32_t uboIndex = 0;
    for (uint32_t objectIndex = 0; objectIndex <scene->objects.objectsCount; objectIndex++)
    {
        auto lookup = scene->transforms._transform_lookup[objectIndex];
        glm::mat4* model = &models[lookup.depth][lookup.index];
        auto subMeshCount = scene->objects.subMeshCtForObject[objectIndex];
        
        transforms[objectIndex].model = *model;
        transforms[objectIndex].Normal = transpose(inverse(glm::mat3(*model)));
        
        for(uint32_t subMeshIndex = 0; subMeshIndex < subMeshCount; subMeshIndex++)
        {
            uint32_t submeshIndex =scene->objects.firstMeshIndex[objectIndex] + subMeshIndex;
            // int i = drawIndices[j];
            Material material = AssetDataAndMemory->materials[scene->objects.materialsForSubmeshes[objectIndex][subMeshIndex]];
        
            perMesh[uboIndex].props.indexInfo = glm::vec4(
            (material.diffuseIndex),
                AssetDataAndMemory->getOffsetFromMeshID(submeshIndex),
                                           (999),
                                          objectIndex);

            perMesh[uboIndex].props.textureInfo = glm::vec4(material.diffuseIndex, material.specIndex, material.normalIndex, -1.0);

            perMesh[uboIndex].props.materialprops = glm::vec4(material.roughness, material.roughness, 0, 0);
            perMesh[uboIndex].props.color = glm::vec4(material.color,1.0f);


        
            //Set position and radius for culling
            //Decompose out scale from the model matrix
            //todo: better to store radius separately and avoid this calculation?
            glm::vec3 scale = glm::vec3(1.0);
            glm::quat _1 = glm::quat();
            glm::vec3 _2 = glm::vec3(0);
            glm::vec3 _3;
            glm::vec4 _4;
            glm::decompose( (*model), scale, _1, _2, _3, _4);
            auto model2 = transpose(*model);


            positionRadius meshSpacePositionAndRadius =  AssetDataAndMemory->meshBoundingSphereRad[submeshIndex];
            float meshRadius = meshSpacePositionAndRadius.radius;
            float objectScale = glm::max(glm::max( scale.x, scale.x), scale.x);
            perMesh[uboIndex].cullingInfo.pos = meshSpacePositionAndRadius.pos;
            meshRadius *= objectScale;
            perMesh[uboIndex].cullingInfo.radius = meshRadius;
            uboIndex++;
        }
    }

    assert(uboIndex == scene->objects.subMeshesCount);
    FramesInFlightData[currentFrame].perMeshbuffers.updateMappedMemory({perMesh.data(), (size_t)scene->objects.subMeshesCount});
    FramesInFlightData[currentFrame].perObjectBuffers.updateMappedMemory({transforms.data(), transforms.size()});



}

void EndCommandBufferRecording(ActiveRenderStepData* context)
{
    assert(context->active);
    VK_CHECK(vkEndCommandBuffer(context->commandBuffer));
    context->active = false;
}

VkRenderingAttachmentInfoKHR CreateRenderingAttatchmentWithLayout(VkImageView target, float clearColor, bool clear, VkImageLayout layout)
{
    return  {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
        .imageView = target,
        .imageLayout = layout,
        .loadOp = clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue =  {clearColor, 0}
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

    
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(extent.width );
    viewport.height = static_cast<float>(extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    
    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = extent;
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
}



//Needs a better name -- these are the higher level render passes, and then meshpasses are pipelines and object lists
//This is everything we need to submit commands
void VulkanRenderer::updateBindingsComputeCulling(ActiveRenderStepData commandBufferContext, 
    MemoryArena::memoryArena* arena, uint32_t _currentFrame)
{
    assert(commandBufferContext.active);
    VkDescriptorBufferInfo* frustumData =  MemoryArena::Alloc<VkDescriptorBufferInfo>(&perFrameArenas[_currentFrame], 1);
    *frustumData = GetDescriptorBufferInfo(FramesInFlightData[_currentFrame].frustumsForCullBuffers);
    
    VkDescriptorBufferInfo* computeDrawBuffer = MemoryArena::Alloc<VkDescriptorBufferInfo>(&perFrameArenas[_currentFrame], 1); 
    *computeDrawBuffer = GetDescriptorBufferInfo(FramesInFlightData[_currentFrame].drawBuffers);

    VkDescriptorBufferInfo* objectBufferInfo = MemoryArena::Alloc<VkDescriptorBufferInfo>(&perFrameArenas[_currentFrame], 1); 
    *objectBufferInfo = GetDescriptorBufferInfo(FramesInFlightData[_currentFrame].perMeshbuffers);

    VkDescriptorBufferInfo* transformBufferInfo = MemoryArena::Alloc<VkDescriptorBufferInfo>(&perFrameArenas[_currentFrame], 1); 
    *transformBufferInfo = GetDescriptorBufferInfo(FramesInFlightData[_currentFrame].perObjectBuffers);
    
    std::span<descriptorUpdateData> descriptorUpdates = MemoryArena::AllocSpan<descriptorUpdateData>(arena, 4);

    descriptorUpdates[0] = {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, frustumData};  //frustum data
    descriptorUpdates[1] = {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, computeDrawBuffer}; //draws 
    descriptorUpdates[2] = {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, objectBufferInfo}; //objectData  //
    descriptorUpdates[3] = {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, transformBufferInfo}; //objectData  //

    DescriptorSets::_updateDescriptorSet_NEW(getMainRendererContext(),pipelineLayoutManager.GetDescriptordata(cullingLayoutIDX, 0)->descriptorSetsCaches[currentFrame].getNextDescriptorSet(),
       pipelineLayoutManager.GetDescriptordata(cullingLayoutIDX, 0)->layoutBindings,  descriptorUpdates); //Update desciptor sets for the compute bindings 

    pipelineLayoutManager.BindRequiredSetsToCommandBuffer(cullingLayoutIDX, commandBufferContext.commandBuffer, commandBufferContext.boundDescriptorSets, _currentFrame, VK_PIPELINE_BIND_POINT_COMPUTE);

    vkCmdBindPipeline(commandBufferContext.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,   pipelineLayoutManager.GetPipeline(cullingLayoutIDX, 0));
}



#pragma endregion

#pragma region draw
void RecordCommandBufferCulling(ActiveRenderStepData commandBufferContext, uint32_t currentFrame, ComputeCullListInfo pass,  VkBuffer indirectCommandsBuffer)
{

    assert(commandBufferContext.active);

    auto drawcount = pass.drawCount;

    vkCmdPushConstants(commandBufferContext.commandBuffer,  pass.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                       pass.pushConstantInfo.size, pass.pushConstantInfo.ptr);

    const uint32_t dispatch_x = drawcount != 0
                                    ? 1 + static_cast<uint32_t>((drawcount - 1) / 16)
                                    : 1;
    vkCmdDispatch(commandBufferContext.commandBuffer, dispatch_x, 1, 1);
     

    
    VkBufferMemoryBarrier2 barrier = bufferBarrier(indirectCommandsBuffer,
    VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
      VK_ACCESS_2_SHADER_WRITE_BIT,
      VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
     VK_ACCESS_2_MEMORY_READ_BIT_KHR );
    
    pipelineBarrier(commandBufferContext.commandBuffer,0, 1, &barrier, 0, 0);

}
void VulkanRenderer::RecordMipChainCompute(ActiveRenderStepData commandBufferContext, Allocator arena, VkImage dstImage,
    VkImageView srcView, std::span<VkImageView> pyramidviews, VkSampler sampler, uint32_t _currentFrame, uint32_t pyramidWidth, uint32_t pyramidHeight)
{
    assert(commandBufferContext.active);
    auto descriptorData = pipelineLayoutManager.GetDescriptordata(mipChainLayoutIDX, 0);
   
    vkCmdBindPipeline(commandBufferContext.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayoutManager.GetPipeline(mipChainLayoutIDX, 0));

    int i = 0;
   
 
    //For each texture, for each mip level, we want to work on, dispatch X/Y texture resolution and set a barrier (for the next phase)
    //Compute shader will read in texture and write back out to it
    VkImageMemoryBarrier2 barrier12 = imageBarrier(dstImage,
        VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_IMAGE_ASPECT_COLOR_BIT,
        i,
        HIZDEPTH);

    
    pipelineBarrier(commandBufferContext.commandBuffer,0, 0, 0, 1, &barrier12);

   
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

        VkDescriptorImageInfo* shadowSamplerInfo =  MemoryArena::Alloc<VkDescriptorImageInfo>(arena);
        *shadowSamplerInfo = {
            .sampler  =sampler, .imageLayout = VK_IMAGE_LAYOUT_GENERAL
        };
  
        std::span<descriptorUpdateData> descriptorUpdates = MemoryArena::AllocSpan<descriptorUpdateData>(arena, 3);

        descriptorUpdates[0] = {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, sourceInfo};  //src view
        descriptorUpdates[1] = {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, destinationInfo};  //dst view
        descriptorUpdates[2] = {VK_DESCRIPTOR_TYPE_SAMPLER, shadowSamplerInfo}; //draws 

        //Memory barrier for compute access
        //Probably not ideal perf
        PipelineMemoryBarrier(commandBufferContext.commandBuffer,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR,
             VK_ACCESS_2_SHADER_WRITE_BIT_KHR,
             VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR,
             VK_ACCESS_2_SHADER_READ_BIT_KHR
              );
        
        DescriptorSets::_updateDescriptorSet_NEW(getMainRendererContext(),mipChainDescriptorCache->getNextDescriptorSet(), mipChainDescriptorConfig->layoutBindings,  descriptorUpdates); //Update desciptor sets for the compute bindings
        pipelineLayoutManager.BindRequiredSetsToCommandBuffer(mipChainLayoutIDX,commandBufferContext.commandBuffer, commandBufferContext.boundDescriptorSets, _currentFrame, VK_PIPELINE_BIND_POINT_COMPUTE);
        

        int outputWidth= pyramidWidth >> i;
        outputWidth = outputWidth < 1 ? 1 : outputWidth;
        int outputHeight = pyramidHeight >> i;
        outputHeight = outputHeight < 1 ? 1 : outputHeight;
        auto pushConstants = MemoryArena::Alloc<glm::vec2>(arena);
        *pushConstants = {outputWidth, outputHeight};
        vkCmdPushConstants(commandBufferContext.commandBuffer, pipelineLayoutManager.GetLayout(mipChainLayoutIDX),VK_SHADER_STAGE_COMPUTE_BIT, 0,
                          sizeof(glm::vec2),pushConstants);

        //Dispatch for all the pixels?
        vkCmdDispatch(commandBufferContext.commandBuffer, outputWidth, outputHeight, 1);

//todo js
        VkImageMemoryBarrier2 barrier = imageBarrier(dstImage,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_MEMORY_WRITE_BIT,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
             VK_ACCESS_SHADER_READ_BIT,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_ASPECT_COLOR_BIT,
            i,
            1);

        pipelineBarrier(commandBufferContext.commandBuffer,0, 0, 0, 1, &barrier);
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
        rendererVulkanObjects.swapchain.extent,commandBuffer);
    
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, opaqueBindlessLayoutGroup);
    // debugLines.push_back({{0,0,0},{20,40,20}, {0,0,1}});
    
    
    for (int i = 0; i <debugLinesManager.debugLines.size(); i++)
    {
    
        debugLinePConstants constants = debugLinesManager.getDebugLineForRendering(i);
        //Fullscreen quad render
        vkCmdPushConstants(commandBuffer, layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                          sizeof(debugLinePConstants), &constants);
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
void RecordIndirectCommandBufferForPasses(Scene* scene, AssetManager* rendererData, MemoryArena::memoryArena* allocator, std::span<drawCommandData> drawCommands, std::span<RenderBatch> passes)
{
    // for(int i =0; i < drawCommands.size(); i++)
    // {
    //     drawCommands[i].objectIndex = -1;
    // }
    
    std::span<uint32_t> subMeshIDtoMeshData = MemoryArena::AllocSpan<uint32_t>(allocator, scene->objects.subMeshesCount);
    std::span<uint32_t> SubmeshIDtoSortedObjectID = MemoryArena::AllocSpan<uint32_t>(allocator, scene->objects.subMeshesCount);
    std::span<uint32_t> meshIDtoFirstIndex = MemoryArena::AllocSpan<uint32_t>(allocator, rendererData->backing_meshes.size());
    std::span<uint32_t> meshIDtoIndexCount = MemoryArena::AllocSpan<uint32_t>(allocator, rendererData->backing_meshes.size());
    uint32_t indexCtoffset = 0;
    uint32_t submeshIdx = 0;
    for(size_t i = 0; i < scene->ObjectsCount(); i++)
    {
        for (size_t j = 0; j < scene->objects.subMeshCtForObject[i]; j++)
        {
            subMeshIDtoMeshData[submeshIdx++] = static_cast<uint32_t>(scene->objects.firstMeshIndex[i] + j);
        }
    }
    for(size_t i = 0; i < scene->objects.subMeshesCount; i++)
    {
        SubmeshIDtoSortedObjectID[i] =  static_cast<uint32_t>(i); //
    }
    for(size_t i = 0; i < rendererData->backing_meshes.size(); i++)
    {
        meshIDtoFirstIndex[i] = indexCtoffset;
        meshIDtoIndexCount[i] =  static_cast<uint32_t>(rendererData->backing_meshes[i].indices.size());
        indexCtoffset +=  static_cast<uint32_t>(rendererData->backing_meshes[i].indices.size());
    }

    
    //indirect draw commands for shadows/depth only draws
    for(size_t i =0; i < passes.size(); i++)
    {
        auto _pass =passes[i];
        for (int j = 0; j < _pass.meshPasses.size(); j++)
        {
            auto pass = _pass.meshPasses[j];
            UpdateDrawCommanddataDrawIndirectCommands(drawCommands.subspan(pass.firstIndex, pass.ct), pass.sortedObjectIDs, subMeshIDtoMeshData,meshIDtoFirstIndex, meshIDtoIndexCount);
        }
    }
}


void RecordPrimaryRenderPasses( std::span<RenderBatch> draws, PipelineLayoutManager* pipelineLayoutManager, VkBuffer indirectCommandsBuffer, VkDescriptorSet perFrameDescriptors, int currentFrame)
{
  
    uint32_t offset_into_struct = offsetof(drawCommandData, command);

    for(int j = 0; j < draws.size(); j++)
    {
        auto passInfo = draws[j];
        RecordCommandBufferCulling(*passInfo.computeRenderStepContext, currentFrame, *passInfo.computeCullingInfo,indirectCommandsBuffer);
    }

    for(int j = 0; j < draws.size(); j++)
    {
        auto passInfo = draws[j];
        auto context = passInfo.drawRenderStepContext;
        assert(context->active);
        pipelineLayoutManager->BindRequiredSetsToCommandBuffer(passInfo.pipelineLayoutGroup, context->commandBuffer, context->boundDescriptorSets, currentFrame);
        VkPipelineLayout layout = pipelineLayoutManager->GetLayout(passInfo.pipelineLayoutGroup);
        if (context->indexBuffer != passInfo.indexBuffer )
        {
            vkCmdBindIndexBuffer(context->commandBuffer, passInfo.indexBuffer,0,passInfo.indexBufferType);
            context->indexBuffer = passInfo.indexBuffer;
        }

        BeginRendering(
            passInfo.depthAttatchment,
            passInfo.colorattatchment == VK_NULL_HANDLE ? 0 : 1, passInfo.colorattatchment,
            passInfo.renderingAttatchmentExtent, context->commandBuffer);
        
        
        if (passInfo.depthBiasSetting.use)
        {
            float baseDepthBias = passInfo.depthBiasSetting.depthBias;
            float baseSLopeBias = passInfo.depthBiasSetting.slopeBias;
            vkCmdSetDepthBias(
                        context->commandBuffer,
                        baseDepthBias,
                        0.0f,
                        baseSLopeBias);
        }
        
        auto meshPasses = passInfo.meshPasses;
        for(uint32_t i = 0; i < meshPasses.size(); i ++)
        {
            auto pipeline = meshPasses[i].pipeline;
            if (context->boundPipeline != pipeline)
            {
                vkCmdBindPipeline(context->commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline);
                context->boundPipeline = pipeline;
            }
            
            uint32_t offset_base =  meshPasses[i].firstIndex  *  sizeof(drawCommandData);
            uint32_t drawIndirectOffset = offset_base + offset_into_struct;
    


            if (passInfo.pushConstantsSize > 0)
            {
                vkCmdPushConstants(context->commandBuffer, layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                                   passInfo.pushConstantsSize, passInfo.pushConstants);
            }

            int meshct =  meshPasses[i].ct;
        
            vkCmdDrawIndexedIndirect(context->commandBuffer,  indirectCommandsBuffer,drawIndirectOffset, meshct, sizeof(drawCommandData));

     
        }
        vkCmdEndRendering(context->commandBuffer);
    }
}


void SubmitCommandBuffer(ActiveRenderStepData* commandBufferContext)
{

    assert(!commandBufferContext->active);
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    VkPipelineStageFlags _waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = (uint32_t)commandBufferContext->waitSemaphores.size();
    submitInfo.pWaitSemaphores = commandBufferContext->waitSemaphores.data();
    submitInfo.pWaitDstStageMask =_waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBufferContext->commandBuffer;

    submitInfo.signalSemaphoreCount = (uint32_t)commandBufferContext->signalSempahores.size();
    submitInfo.pSignalSemaphores = commandBufferContext->signalSempahores.data();
    
    //Submit pass 
    VK_CHECK(vkQueueSubmit(commandBufferContext->Queue, 1,
        &submitInfo, commandBufferContext->fence != nullptr? *commandBufferContext->fence : VK_NULL_HANDLE));
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
    DescriptorSets::_updateDescriptorSet_NEW(getMainRendererContext(), perSceneDescriptorData->descriptorSetsCaches[0].getNextDescriptorSet(), perSceneDescriptorData->layoutBindings,perSceneDescriptorUpdates);
}
void VulkanRenderer::Update(Scene* scene)
{
    ImGui::Begin("RENDERER Buffer preview window");
    ImGui::Text("TODO");
    int shadowIndex =1;
    VkDescriptorSet img = GetOrRegisterImguiTextureHandle(shadowResources.shadowSamplers[shadowIndex],shadowResources.shadowMapSamplingImageViews[shadowIndex][3]);
    //todo js: need to change image before/after doing this? need to do this toa  copy? not sure -- get valdidation errors around layout
    // VkDescriptorSet img_depth_0 = GetOrRegisterImguiTextureHandle(rendererResources.depthMipSampler,rendererResources.depthBufferInfo.view);
    // VkDescriptorSet img_depth_1 = GetOrRegisterImguiTextureHandle(rendererResources.depthMipSampler,rendererResources.depthPyramidInfo.viewsForMips[4]);
    //
    // ImGui::Image(ImTextureID(img_depth_0), ImVec2{480, 480});
    // ImGui::Image(ImTextureID(img_depth_1), ImVec2{480, 480});
    ImGui::End();
    
    //make imgui calculate internal draw structures
    //Note -- we're not just submitting imgu icalls here, some are from the higher level of the app
    ImGui::Render();

    auto checkFences = !firstRunOfFrame[currentFrame];
    if (checkFences)
    {

        printf("begin wait for fence %d \n", currentFrame);
        clock_t before = clock();
        vkWaitForFences(rendererVulkanObjects.vkbdevice.device, 1, &FramesInFlightData[currentFrame].inFlightFence, VK_TRUE, UINT64_MAX);
        clock_t afterFence = clock();
        clock_t difference = clock() - afterFence;
        auto msec = difference * 1000 / CLOCKS_PER_SEC;

        //Render
        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount =  0;
        presentInfo.pWaitSemaphores = {};
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = {&rendererVulkanObjects.swapchain.swapchain};
        presentInfo.pImageIndices = &FramesInFlightData[currentFrame].swapChainIndex;
        presentInfo.pResults = nullptr; // Optional

        //Present e
        vkQueuePresentKHR(GET_QUEUES()->presentQueue, &presentInfo);
        
        printf("end wait for fence %d, MS %d \n", currentFrame, msec);
        
       
        perFrameDeletionQueuse[currentFrame]->FreeQueue();
        MemoryArena::free(&perFrameArenas[currentFrame]);
   
        vkResetFences(rendererVulkanObjects.vkbdevice.device, 1, &FramesInFlightData[currentFrame].inFlightFence);
  
    }
    if (!checkFences)
    {
        firstRunOfFrame[currentFrame] = false;
    }
    uint64_t RenderLoop =SDL_GetPerformanceCounter();
    AssetDataAndMemory->Update();

    //This stuff belongs here
    pipelineLayoutManager.ResetForFrame(currentFrame);
    RenderFrame(scene);
    debugLinesManager.debugLines.clear();

    auto t2 = SDL_GetPerformanceCounter();
    auto difference =SDL_GetPerformanceCounter() - RenderLoop;
    float msec = ((1000.0f * difference) / SDL_GetPerformanceFrequency());
    printf("end render loop %d, MS %ld \n", currentFrame, (long)msec);
    
    // vkDeviceWaitIdle(rendererVulkanObjects.vkbdevice.device);
}

VkPipelineStageFlags swapchainWaitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
VkPipelineStageFlags shadowWaitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

bool GetFlattenedShadowDataForIndex(int index, std::span<std::span<PerShadowData>> inputShadowdata, PerShadowData* result)
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



RenderBatchCreationConfig  CreateOpaquePassesConfig(uint32_t firstObject, std::span<FullShaderHandle> shaderGroup, CommonRenderPassData passData,
                                     std::span<std::span<PerShadowData>> inputShadowdata,
                                     PipelineLayoutHandle layoutGroup,
                                     ActiveRenderStepData* opaqueRenderStepContext,
                                     VkRenderingAttachmentInfoKHR* opaqueTarget,
                                     VkRenderingAttachmentInfoKHR* depthTarget,
                                     VkExtent2D targetExtent
                                     )
    
{
    uint32_t objectsPerDraw = (uint32_t)passData.scenePtr->objects.subMeshesCount;
    uint32_t nextFirstObject =firstObject;
    viewProj viewProjMatricesForCulling = LightAndCameraHelpers::CalcViewProjFromCamera(passData.scenePtr->sceneCamera);
    //Culling debug stuff
    PerShadowData* data = MemoryArena::Alloc<PerShadowData>(passData.tempAllocator);
    size_t shadowDrawCt = 0;
    for(int i =0; i <inputShadowdata.size(); i++)
    {
        shadowDrawCt += inputShadowdata[i].size();
    }
    // assert(targetPassList->drawCount >= shadowDrawCt); //TODO JS-frustum-binding

    
    if ( debug_cull_override && GetFlattenedShadowDataForIndex(debug_cull_override_index,inputShadowdata, data))
    {
        viewProjMatricesForCulling = { data->view,  data->proj};
        glm::vec4 frustumCornersWorldSpace[8] = {};
        LightAndCameraHelpers::FillFrustumCornersForSpace(frustumCornersWorldSpace,glm::inverse(viewProjMatricesForCulling.proj * viewProjMatricesForCulling.view));
        debugLinesManager.AddDebugFrustum(frustumCornersWorldSpace);
    }
    
     RenderBatchCreationConfig c  = {
        .name = const_cast<char*>("opaque"),
        .attatchmentInfo =  {.colorDraw = opaqueTarget, .depthDraw = depthTarget,
            .extents = targetExtent},
        .shaderGroup =shaderGroup,
        .layoutGroup = layoutGroup,
        .StepContext = opaqueRenderStepContext,
        .pushConstant = {.ptr = nullptr, .size = 0},
        .cameraViewProjForCulling = viewProjMatricesForCulling,
        .drawOffset = nextFirstObject,
        .objectCount = objectsPerDraw,
        .depthBiasConfig = {false, 0, 0}
    };
    return c;

   

}

std::span<RenderBatchCreationConfig> CreateShadowPassConfigs(uint32_t firstObject, CommonRenderPassData config,
                                     cameraData* camera, 
                                     std::span<std::span<PerShadowData>> inputShadowdata,
                                     PipelineLayoutHandle layoutHandle,
                                    std::span<FullShaderHandle> shaderLookup,
                                     ActiveRenderStepData* shadowRenderStepContext,
                                     std::span<VkImageView> shadowMapRenderingViews)
{
    uint32_t objectsPerDraw = (uint32_t)config.scenePtr->objects.subMeshesCount;
    uint32_t nextFirstObject =firstObject;

    auto shadowCasterCt =  glm::min(config.scenePtr->lightCount, MAX_SHADOWCASTERS); 
    Array resultConfigs = MemoryArena::AllocSpan<RenderBatchCreationConfig>(config.tempAllocator, shadowCasterCt * 6);
    for(size_t i = 0; i < glm::min(config.scenePtr->lightCount, MAX_SHADOWCASTERS); i ++)
    {
        lightType type = (lightType)config.scenePtr->lightTypes[i];
        size_t lightSubpasses = shadowCountFromLightType(type);
        for (size_t j = 0; j < lightSubpasses; j++)
        {
            auto view = inputShadowdata[i][j].view;
            auto proj =  inputShadowdata[i][j].proj;
            //todo js: wanna rethink this, since shadow rendering is like effectively dupe with depth prepass rendering 
            shadowPushConstants* shadowPC = MemoryArena::Alloc<shadowPushConstants>(config.tempAllocator);
            shadowPC ->matrix =  proj * view;
            
            VkRenderingAttachmentInfoKHR* depthDrawAttatchment = MemoryArena::Alloc<VkRenderingAttachmentInfoKHR>(config.tempAllocator);
            *depthDrawAttatchment =  CreateRenderingAttatchmentStruct(shadowMapRenderingViews[resultConfigs.size()], 1.0, true);

            RenderBatchCreationConfig c = {
                .name = const_cast<char*>("shadow"),
                .attatchmentInfo = {.colorDraw = VK_NULL_HANDLE, .depthDraw = depthDrawAttatchment,
                    .extents = {SHADOW_MAP_SIZE, SHADOW_MAP_SIZE}},
                .shaderGroup =shaderLookup,
                .layoutGroup = layoutHandle,
                .StepContext = shadowRenderStepContext,
                .pushConstant = {.ptr = shadowPC, .size =256},
                .cameraViewProjForCulling = {.view = view, .proj =proj },
                .frustumIndex = (uint32_t)resultConfigs.size(),
                .drawOffset = nextFirstObject,
                .objectCount = objectsPerDraw,
                .depthBiasConfig = {.use = true, .depthBias = 6.0, .slopeBias = 3.0}
            };
            nextFirstObject += objectsPerDraw;
            resultConfigs.push_back(c);
        }
    }

    return resultConfigs.getSpan();
}

size_t UpdateDrawCommanddataDrawIndirectCommands(std::span<drawCommandData> targetDrawCommandSpan, std::span<uint32_t> submeshIDstoSortedSubmeshIDs, std::span<uint32_t> submeshIDtoMeshID, std::span<uint32_t>meshIDtoFirstIndex, std::span<uint32_t> meshIDtoIndexCount)
{
    size_t drawIndex = 0;
    for (size_t j = 0; j < submeshIDstoSortedSubmeshIDs.size(); j++)
    {
        
            auto objectDrawIndex = submeshIDstoSortedSubmeshIDs[j];
            auto meshID = submeshIDtoMeshID[objectDrawIndex];
            targetDrawCommandSpan[drawIndex++] = {
                (uint32_t)objectDrawIndex,
                {
                    .indexCount = meshIDtoIndexCount[meshID],
                    .instanceCount = 1,
                    .firstIndex = meshIDtoFirstIndex[meshID],
                    .vertexOffset = 0,
                    .firstInstance = (uint32_t)objectDrawIndex
                }
            };
    }
    return  targetDrawCommandSpan.size() ;
}


ActiveRenderStepData CreateAndBeginRenderStep(VkDevice device, const char* debugName, const char* cbufferDebugName,  RendererDeletionQueue* deletionQueue, VkCommandPool pool,     std::span<VkSemaphore> waitSemaphores,
    std::span<VkSemaphore> signalSempahores)
{
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = pool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;
    
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer));
    setDebugObjectName(device,VK_OBJECT_TYPE_COMMAND_BUFFER, cbufferDebugName, (uint64_t)commandBuffer);

    for(int i = 0; i < signalSempahores.size(); i++) 
    {
        auto sem = &signalSempahores[i];
        createSemaphore(device, sem, debugName, deletionQueue);
        setDebugObjectName(device, VK_OBJECT_TYPE_SEMAPHORE,debugName, (uint64_t)*sem);
    }

    //Begin command buffer
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = 0; // Optional
    beginInfo.pInheritanceInfo = nullptr; // Optional
    
    VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

    return{.active = true, .indexBuffer = VK_NULL_HANDLE, .boundPipeline = VK_NULL_HANDLE,
        .commandBuffer = commandBuffer, .waitSemaphores = waitSemaphores, .signalSempahores = signalSempahores };
}

VkDescriptorSet UpdateAndGetBindlessDescriptorSetForFrame(PerThreadRenderContext context, DescriptorDataForPipeline* descriptorData, int currentFrame,  std::span<descriptorUpdates> updates)
{
    size_t idx = descriptorData->isPerFrame ? currentFrame : 0;
    auto descriptorSet = descriptorData->descriptorSetsCaches[idx].getNextDescriptorSet();
    DescriptorSets::_updateDescriptorSet_NEW(context, descriptorSet, descriptorData->layoutBindings,updates[idx]);
    return descriptorSet;
}
uint32_t internal_debug_cull_override_index = 0;

uint32_t GetNextFirstIndex(RenderBatchQueue* q)
{
    if (q->batchConfigs.size() == 0 || q->batchConfigs.back().meshPasses.size() == 0)
    {
        return 0;
    }
    auto& pass =  q->batchConfigs.back().meshPasses.back();
    return pass.firstIndex + pass.ct;
}
void VulkanRenderer::RenderFrame(Scene* scene)
{
    if (debug_cull_override_index != internal_debug_cull_override_index)
    {
        debug_cull_override_index %= Scene::getShadowDataIndex(glm::min(scene->lightCount, MAX_SHADOWCASTERS), scene->lightTypes.getSpan());
        internal_debug_cull_override_index = debug_cull_override_index;
        printf("new cull index: %d! \n",  debug_cull_override_index);
    }
    //Update per frame data

    //Prepare for pass
    per_frame_data* thisFrameData = &FramesInFlightData[currentFrame];
    
    acquireImageSemaphore acquireImageSemaphore = thisFrameData->semaphores;
    vkAcquireNextImageKHR(rendererVulkanObjects.vkbdevice.device, rendererVulkanObjects.swapchain,
        6000,acquireImageSemaphore.semaphore, VK_NULL_HANDLE,&thisFrameData->swapChainIndex);
    
    //Sync data updated from the engine
    updateShadowData(&perFrameArenas[currentFrame], perLightShadowData, scene, scene->sceneCamera);
    
    //Update per frame descriptor sets
    VkDescriptorSet PerFrameBindlessDescriptorSet = UpdateAndGetBindlessDescriptorSetForFrame(getMainRendererContext(), pipelineLayoutManager.GetDescriptordata(opaqueLayoutIDX, 1),currentFrame,perFrameDescriptorUpdates);

    updatePerFrameBuffers(currentFrame, scene->transforms.worldMatrices,scene); 

    

    //Kinda a convenience thing, todo more later, wrote in a hurry
    struct ComandBufferSubmissionPipeline
    {
        VkDevice device;
        RendererDeletionQueue* deletionQueue;
        Array<ActiveRenderStepData> renderstepDatas;
        Array<VkSemaphore> semaphorePool;
        std::span<VkSemaphore> previousStepSignalSemaphores;

        void EndCommandBuffer(ActiveRenderStepData* data)
        {
            assert(data->active);
            VK_CHECK(vkEndCommandBuffer(data->commandBuffer));
            data->active = false;
        }
        void _SubmitCommandBuffer(ActiveRenderStepData* ctx)
        {
            SubmitCommandBuffer(ctx);
        }

        void FinishAndSubmitAll()
        {
            for (auto& past_time : renderstepDatas.getSpan())
            {
                EndCommandBuffer(&past_time);
                _SubmitCommandBuffer(&past_time);
            }
        }
        //Set up a command buffer, initialize it, return an object wrapping all the relevant stuff
        //Default to chaining semaphores with the last/next one
        ActiveRenderStepData* PushAndInitializeRenderStep(const char* debugName,  const char* cbufferDebugName, MemoryArena::memoryArena* arena, CommandPoolManager* poolManager,
            bool overrideWaitSemaphores= false , std::span<VkSemaphore> overridenWaitSemaphores = {}, //Overhead for empty spans? Fix someday
             bool overrideSignalSemaphores = false , std::span<VkSemaphore> overridenSignalSemaphores = {}) //Overhead for empty spans? Fix someday
        {
            assert(!(!overrideWaitSemaphores && renderstepDatas.ct == 0));  //We can only override wait semaphores if we have prior steps. 
            
            auto signalSemaphores = overrideSignalSemaphores ? overridenSignalSemaphores : semaphorePool.pushUninitializedSubspan(1);
            auto waitSemaphores = overrideWaitSemaphores ? overridenWaitSemaphores : previousStepSignalSemaphores;
            previousStepSignalSemaphores = signalSemaphores;
            VkCommandBufferAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            allocInfo.commandPool = poolManager->commandPool;
            allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            allocInfo.commandBufferCount = 1;
    
            VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
            VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer));
            setDebugObjectName(device,VK_OBJECT_TYPE_COMMAND_BUFFER, cbufferDebugName, (uint64_t)commandBuffer);

            for(int i = 0; i < signalSemaphores.size(); i++) 
            {
                auto sem = &signalSemaphores[i];
                createSemaphore(device, sem, debugName, deletionQueue);
                setDebugObjectName(device, VK_OBJECT_TYPE_SEMAPHORE,debugName, (uint64_t)*sem);
            }

            //Begin command buffer
            VkCommandBufferBeginInfo beginInfo{};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            beginInfo.flags = 0; // Optional
            beginInfo.pInheritanceInfo = nullptr; // Optional
    
            VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));
            auto boundDescriptorSets = MemoryArena::AllocSpan<VkDescriptorSet>(arena, 4);
            for (int i = 0; i <boundDescriptorSets.size(); i++)
            {
                boundDescriptorSets[i] = VK_NULL_HANDLE;
            }
            renderstepDatas.push_back({.active = true, .indexBuffer = VK_NULL_HANDLE, .boundPipeline = VK_NULL_HANDLE,
                .commandBuffer = commandBuffer, .Queue =GET_QUEUES()->graphicsQueue, .waitSemaphores = waitSemaphores, .signalSempahores = signalSemaphores, .boundDescriptorSets = boundDescriptorSets });

            return &renderstepDatas.back();
        }


        
    };

    ComandBufferSubmissionPipeline renderCommandBufferObjects =
        {
        .device =   rendererVulkanObjects.vkbdevice.device,
        .deletionQueue = perFrameDeletionQueuse[currentFrame].get(),
        .renderstepDatas =  MemoryArena::AllocSpan<ActiveRenderStepData>(&perFrameArenas[currentFrame], 20),
        .semaphorePool = MemoryArena::AllocSpan<VkSemaphore>(&perFrameArenas[currentFrame], 100)
        };

    //I'm currently using multiple command buffers to handle most of my concurrency
    //each one of these step datas gets a signal and wait semaphore list, and then their command buffers are all submitted in order in the loop near the end of this fn
    //This needs revisted, but not right away
    //Create command buffers for this frame and allocate their signal semaphores/set up their dependencies. Written here in linear order

    /////////<Set up command buffers
    //////////
    /////////
    auto preRenderTransitionsrenderStepContext = renderCommandBufferObjects.PushAndInitializeRenderStep(
        "swapTransitionInSignalSemaphores", "swapTransitionInCommandBuffer", &perFrameArenas[currentFrame],
        commandPoolmanager.get(),
        true, std::span<VkSemaphore>(&acquireImageSemaphore.semaphore, 1));

    auto computeRenderStepContext = renderCommandBufferObjects.PushAndInitializeRenderStep(
        "frustumSignalSemaphores", "frustumCommandBuffer", &perFrameArenas[currentFrame],
        commandPoolmanager.get());

    auto cullingDepthPrepassRenderStepContext = renderCommandBufferObjects.PushAndInitializeRenderStep(
        "depthPrepassSignalSemaphores", "depthPrepassCommandBuffer", &perFrameArenas[currentFrame],
        commandPoolmanager.get());

    auto mipChainRenderStepContext = renderCommandBufferObjects.PushAndInitializeRenderStep(
        "MipChainSignalSemaphores", "MipChainCommandBuffer", &perFrameArenas[currentFrame],
        commandPoolmanager.get());
    
    auto shadowRenderStepContext = renderCommandBufferObjects.PushAndInitializeRenderStep(
        "ShadowAndBeforeOpaqueSemaphores", "ShadowAndBeforeOpaqueCommandBuffer", &perFrameArenas[currentFrame],
        commandPoolmanager.get());
    
    auto& beforeOpaqueTransitionOutRenderStepContext = shadowRenderStepContext;
    
    auto opaqueRenderStepContext = renderCommandBufferObjects.PushAndInitializeRenderStep(
        "OpaqueAndTransitionOutSignalSemaphores", "OpaqueAndTransitionOutCommandBuffer", &perFrameArenas[currentFrame],
        commandPoolmanager.get());

    //TODO JS: the fence is very important I think -- need to understand what i was doing when i added it lol
    //TODO JS: Mutating the opaqueRenderStepContext like this doesnt feel amazing, would love to set this all in creation
    
    auto& postRenderTransitionOutStepContext = opaqueRenderStepContext;

    opaqueRenderStepContext->fence = &FramesInFlightData[currentFrame].inFlightFence;
    //Pre-rendering setup
    //  // //
    // TransitionImageForRendering(
    // getMainRendererContext(),
    // preRenderTransitionsrenderStepContext,
    // globalResources.swapchainImages[swapChainImageIndex],
    // VK_IMAGE_LAYOUT_UNDEFINED,
    // VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, swapchainWaitStages, 1, false);

    VkImageMemoryBarrier2 barrier = imageBarrier(globalResources.swapchainImages[thisFrameData->swapChainIndex],
       VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, // todo, top of pipe?
        VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
       VK_IMAGE_LAYOUT_UNDEFINED,
       VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT ,
        VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
       VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
       VK_IMAGE_ASPECT_COLOR_BIT,
       0,
       VK_REMAINING_ARRAY_LAYERS);

    
    pipelineBarrier(preRenderTransitionsrenderStepContext->commandBuffer,0, 0, 0, 1, &barrier);

    
    VkImageMemoryBarrier2 barrier2 = imageBarrier(shadowResources.shadowImages[currentFrame],
       VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, // todo, top of pipe?
       VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
       VK_IMAGE_LAYOUT_UNDEFINED,
       VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT ,
       VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT|VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
       VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
       VK_IMAGE_ASPECT_DEPTH_BIT,
       0,
       VK_REMAINING_ARRAY_LAYERS);

    
    pipelineBarrier(preRenderTransitionsrenderStepContext->commandBuffer,0, 0, 0, 1, &barrier2);
    
    
    //Transferring cpu -> gpu data -- should improve
    AddBufferTrasnfer(thisFrameData->hostVerts.buffer.data, thisFrameData->deviceVerts.data,
                      thisFrameData->deviceVerts.size, preRenderTransitionsrenderStepContext->commandBuffer);
    AddBufferTrasnfer(thisFrameData->hostMesh.buffer.data, thisFrameData->deviceMesh.data,
                      thisFrameData->deviceMesh.size, preRenderTransitionsrenderStepContext->commandBuffer);
    AddBufferTrasnfer(thisFrameData->hostIndices.buffer.data, thisFrameData->deviceIndices.data,
                      thisFrameData->deviceIndices.size, preRenderTransitionsrenderStepContext->commandBuffer);

    //Set up draws
    //This is the bulk of the render, the main opaque pass which depends on compute, shadow, opaque, culling, etc
        //Need to make a new "aggregate step" concept to handle this so I can make it a lambda like the rest
    RecordMipChainCompute(*mipChainRenderStepContext, &perFrameArenas[currentFrame],  globalResources.depthPyramidInfoPerFrame[currentFrame].image,
                          globalResources.depthBufferInfoPerFrame[currentFrame].view, globalResources.depthPyramidInfoPerFrame[currentFrame].viewsForMips, globalResources.depthMipSampler, currentFrame, globalResources.depthPyramidInfoPerFrame[currentFrame].depthSize.x,
                          globalResources.depthPyramidInfoPerFrame[currentFrame].depthSize.y);     

    //Create the renderpass
     auto* mainRenderTargetAttatchment = MemoryArena::Alloc<VkRenderingAttachmentInfoKHR>(&perFrameArenas[currentFrame]);
     *mainRenderTargetAttatchment =  CreateRenderingAttatchmentStruct(globalResources.swapchainImageViews[thisFrameData->swapChainIndex], 0.0, true);

    //set up all our  draws
    RenderBatchQueue renderBatches  = {.drawCount = 0,  .batchConfigs =  {}};

    CommonRenderPassData renderPassContext =
        {
        .tempAllocator =  &perFrameArenas[currentFrame],
        .scenePtr = scene,
        .assetDataPtr =  AssetDataAndMemory,
        .cullLayout =  pipelineLayoutManager.GetLayout(cullingLayoutIDX),
        .pipelineManagerPtr =  &pipelineLayoutManager,
        .computeRenderStepContext =computeRenderStepContext,
        .indexBuffer = thisFrameData->deviceIndices.data
        };


     auto shadowPassConfigs = CreateShadowPassConfigs(GetNextFirstIndex(&renderBatches), renderPassContext, &scene->sceneCamera,
                     perLightShadowData, shadowLayoutIDX,  opaqueObjectShaderSets.shadowShaders.getSpan(),
                     shadowRenderStepContext, shadowResources.shadowMapRenderingImageViews[currentFrame]);

    for (auto shadow_conf : shadowPassConfigs)
    {
        renderBatches.EmplaceBatch(
            shadow_conf.name,&renderPassContext,shadow_conf
        );
    }
    VkRenderingAttachmentInfoKHR* depthDrawAttatchment = MemoryArena::Alloc<VkRenderingAttachmentInfoKHR>(&perFrameArenas[currentFrame]);
    *depthDrawAttatchment = CreateRenderingAttatchmentStruct( globalResources.depthBufferInfoPerFrame[currentFrame].view, 1.0, true);
    
    auto conf = CreateOpaquePassesConfig(GetNextFirstIndex(&renderBatches),opaqueObjectShaderSets.opaqueShaders.getSpan(), renderPassContext, 
                    perLightShadowData, opaqueLayoutIDX, 
                    opaqueRenderStepContext,mainRenderTargetAttatchment,depthDrawAttatchment, rendererVulkanObjects.swapchain.extent);

    renderBatches.EmplaceBatch(
        conf.name,&renderPassContext,conf
    );



    //Indirect command buffer
    RecordIndirectCommandBufferForPasses(scene, AssetDataAndMemory, &perFrameArenas[currentFrame], thisFrameData->drawBuffers.getMappedSpan(), renderBatches.batchConfigs);
    updateBindingsComputeCulling(*computeRenderStepContext,  &perFrameArenas[currentFrame], currentFrame);
    
 
    //Prototype depth passes code
    //Going to move creation of these into the add passes fns, and have the logic to look up the appropriate sahder passed thru there
    //Need to think about how 
    auto batches = renderBatches.batchConfigs;
    for(size_t i = 0; i < batches.size(); i++)
    {
        auto oldPass = batches[i];
        auto newPass =  batches[i];
        newPass.colorattatchment = VK_NULL_HANDLE;
        std::span<simpleMeshPassInfo> oldPasses = batches[i].meshPasses;
        std::span<simpleMeshPassInfo> newPasses = MemoryArena::copySpan<simpleMeshPassInfo>(&perFrameArenas[currentFrame], oldPasses);
        for(int j =0; j < newPasses.size(); j++)
        {
            newPasses[j].pipeline = pipelineLayoutManager.GetPipeline(SHADOW_PREPASS_SHADER_INDEX); 
        }
        newPass.debugName = std::string("depth");
        newPass.meshPasses = newPasses;
        newPass.drawRenderStepContext = cullingDepthPrepassRenderStepContext;
        newPass.depthAttatchment = MemoryArena::AllocCopy<VkRenderingAttachmentInfoKHR>(&perFrameArenas[currentFrame], *batches[i].depthAttatchment);
        if (newPass.pushConstantsSize == 0) //kludge -- this is a opaque draw
        {
            newPass.depthBiasSetting = {true, 0, 0};
            for(int j =0; j < newPasses.size(); j++)
            {
                newPasses[j].pipeline = pipelineLayoutManager.GetPipeline(OPAQUE_PREPASS_SHADER_INDEX); 
            }
        }
        renderBatches.batchConfigs.push_back(newPass);
        batches[i].depthAttatchment->loadOp = VK_ATTACHMENT_LOAD_OP_LOAD; ////TODO JS: as I work on depth prepass -- later set this when the pass is created
    }


  
    //Submti the actual Draws
    RecordPrimaryRenderPasses(renderBatches.batchConfigs, &pipelineLayoutManager, thisFrameData->drawBuffers.buffer.data,PerFrameBindlessDescriptorSet, currentFrame);
    
    RecordUtilityPasses(opaqueRenderStepContext->commandBuffer, currentFrame);
    //

    //After render steps
    //
    //
    
    TransitionImageForRendering(
      getMainRendererContext(),
      beforeOpaqueTransitionOutRenderStepContext,
      shadowResources.shadowImages[currentFrame],
      VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, shadowWaitStages, 1, true);
    TransitionImageForRendering(
        getMainRendererContext(),
        beforeOpaqueTransitionOutRenderStepContext,
        globalResources.swapchainImages[thisFrameData->swapChainIndex],
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, swapchainWaitStages, 1, false);


    // //Submit commandbuffers
    renderCommandBufferObjects.FinishAndSubmitAll();
    // for (int j = 0; j < renderCommandBufferObjects.renderstepDatas.size(); j++)
    // {
    //     ActiveRenderStepData* ctx = &renderCommandBufferObjects.renderstepDatas[j];
    //     EndCommandBufferRecording(ctx);
    //     SubmitCommandBuffer(1, ctx, ctx->fence != nullptr? *ctx->fence : VK_NULL_HANDLE);
    // }


 

    
    printf("submit render%d %d\n", clock(), currentFrame);
    

    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;

}

void VulkanRenderer::Cleanup()
{
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        vkWaitForFences(rendererVulkanObjects.vkbdevice.device, 1, &FramesInFlightData[i].inFlightFence, VK_TRUE,
                        UINT64_MAX);
    }

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    deletionQueue->FreeQueue();
    pipelineLayoutManager.cleanup();
    AssetDataAndMemory->Cleanup(); //noop currently
    destroy_swapchain(rendererVulkanObjects.swapchain);

    vkDestroySurfaceKHR(rendererVulkanObjects.vkbInstance.instance, rendererVulkanObjects.surface, nullptr);

    vkDeviceWaitIdle(rendererVulkanObjects.vkbdevice.device);
    vmaDestroyAllocator(rendererVulkanObjects.vmaAllocator);
    destroy_device(rendererVulkanObjects.vkbdevice);
    destroy_instance(rendererVulkanObjects.vkbInstance);
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

//could be static utility, move
//Throw a bunch of VUID-vkCmdDrawIndexed-viewType-07752 errors due to image format, but renders fine
VkDescriptorSet VulkanRenderer::GetOrRegisterImguiTextureHandle(VkSampler sampler, VkImageView imageView)
{
        if (imguiRegisteredTextures.contains(imageView))
        {
            return imguiRegisteredTextures.at(imageView);
        }
  
    //auto img = ImGui_ImplVulkan_AddTexture(shadowSamplers[shadowIndex], shadowMapSamplingImageViews[shadowIndex][0], VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL);
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

    VK_CHECK(vkCreateDescriptorPool(rendererVulkanObjects.vkbdevice.device, &pool_info, nullptr, &imgui_descriptorPool));
    deletionQueue->push_backVk(deletionType::descriptorPool, uint64_t(imgui_descriptorPool));

    ImGui_ImplSDL2_InitForVulkan(_window);
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = rendererVulkanObjects.vkbInstance.instance;
    init_info.PhysicalDevice = rendererVulkanObjects.vkbPhysicalDevice.physical_device;
    init_info.Device = rendererVulkanObjects.vkbdevice.device;
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
    init_info.PipelineRenderingCreateInfo.pColorAttachmentFormats = &rendererVulkanObjects.swapchain.image_format;
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

