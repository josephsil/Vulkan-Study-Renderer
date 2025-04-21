
#include "vulkanRenderer.h"
#include<glm/gtc/quaternion.hpp>
#undef main
#define SDL_MAIN_HANDLED 
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
////
#include <cstdlib>
#include <functional>
#include <span>
#include "../Vertex.h"
#include <ImageLibraryImplementations.h>
#include <General/Array.h>
#include <General/MemoryArena.h>
#include "../bufferCreation.h"
#include "../CommandPoolManager.h"
#include "../gpu-data-structs.h"
#include "../meshData.h"
#include <Scene/AssetManager.h>
#include "glm_misc.h"
#include "imgui.h"
#include "../RendererInterface.h"
#include "../textureCreation.h"
#include "../TextureData.h"
#include "../vulkan-utilities.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_vulkan.h"
#include "Scene/Scene.h"
#include "Scene/Transforms.h"
#include "../VulkanIncludes/vmaImplementation.h"
#include "../VulkanIncludes/VulkanMemory.h"
#include "General/InputHandling.h"
#include "Renderer/VulkanIncludes/Vulkan_Includes.h"

struct gpuPerShadowData;


#pragma region vkb setup


#pragma endregion



std::vector<unsigned int> pastTimes;
unsigned int averageCbTime;
unsigned int frames;
unsigned int MAX_TEXTURES = 120;

//Slowly making this less of a giant class that owns lots of things and moving to these static FNS -- will eventually migrate thewm
RendererResources /*todo*/  static_initializeResources(rendererObjects initializedrenderer,  MemoryArena::memoryArena* allocationArena, RendererDeletionQueue* deletionQueue,  CommandPoolManager* commandPoolmanager);
DepthBufferInfo static_createDepthResources(rendererObjects initializedrenderer,  MemoryArena::memoryArena* allocationArena, RendererDeletionQueue* deletionQueue,  CommandPoolManager* commandPoolmanager);
DepthPyramidInfo static_createDepthPyramidResources(rendererObjects initializedrenderer,  MemoryArena::memoryArena* allocationArena, RendererDeletionQueue* deletionQueue, CommandPoolManager* commandPoolmanager );
void static_createSyncObjects(VkDevice device,  RendererDeletionQueue* deletionQueue, acquireImageSemaphore* semaphoresToCreate, VkFence* fence);

vulkanRenderer::vulkanRenderer()
{
    //Initialize memory arenas we use throughout renderer 
    this->rendererArena = {};
    MemoryArena::initialize(&rendererArena, 1000000 * 200); // 200mb
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        MemoryArena::initialize(&perFrameArenas[i], 100000 * 50);
        // 5mb each //TODO JS: Could be much smaller if I used stable memory for per frame verts and stuff
    }


    FramesInFlightData.resize(MAX_FRAMES_IN_FLIGHT);

    initializeWindow();

    rendererVulkanObjects = static_initializeRendererHandles(_window, WIDTH, HEIGHT);

    //Initialize the main datastructures the renderer uses

    this->deletionQueue = std::make_unique<RendererDeletionQueue>(rendererVulkanObjects.vkbdevice, rendererVulkanObjects.vmaAllocator);
    for (int i = 0; i < rendererResources.swapchainImageViews.size(); i++)
    {
        deletionQueue->push_backVk(deletionType::ImageView, uint64_t(rendererResources.swapchainImageViews[i]));
    }

    commandPoolmanager = std::make_unique<CommandPoolManager>(rendererVulkanObjects.vkbdevice);
    deletionQueue->push_backVk(deletionType::CommandPool, uint64_t(commandPoolmanager->commandPool));
    deletionQueue->push_backVk(deletionType::CommandPool, uint64_t(commandPoolmanager->transferCommandPool));

    rendererResources = static_initializeResources(rendererVulkanObjects, &rendererArena, deletionQueue.get(),
                                                   commandPoolmanager.get());

    //Command buffer stuff
    //semaphores
    for (int i = 0; i < FramesInFlightData.size(); i++)
    {
        static_createSyncObjects(rendererVulkanObjects.vkbdevice.device, deletionQueue.get(),
                                 &(FramesInFlightData[i].semaphores), &FramesInFlightData[i].inFlightFence);
        FramesInFlightData[i].deletionQueue = std::make_unique<RendererDeletionQueue>(rendererVulkanObjects.vkbdevice, rendererVulkanObjects.vmaAllocator);
    }


    //Initialize sceneData
    AssetDataAndMemory = MemoryArena::Alloc<AssetManager>(&rendererArena);
    static_AllocateAssetMemory(&rendererArena, AssetDataAndMemory);


    //imgui
    initializeDearIMGUI();

    //finally
    pastTimes.resize(9999);
}


TextureData cube_irradiance;
TextureData cube_specular;

// VkSurfaceKHR surface; //todo js 0
// vkb::Instance vkb_instance; //todo js 0
// vkb::PhysicalDevice vkb_physicalDevice; //todo js 0
// vkb::Device vkb_device; //todo js 0
// vkb::Swapchain vkb_swapchain; //todo js 0


RendererContext vulkanRenderer::getFullRendererContext()
{
    return RendererContext{
        .physicalDevice = rendererVulkanObjects.vkbdevice.physical_device,
        .device =  rendererVulkanObjects.vkbdevice.device,
        .textureCreationcommandPoolmanager = commandPoolmanager.get(),
        .allocator =  rendererVulkanObjects.vmaAllocator,
        .arena = &rendererArena,
        .tempArena = &perFrameArenas[currentFrame],
        .rendererdeletionqueue = deletionQueue.get()};
}

BufferCreationContext vulkanRenderer::getRendererForVkObjects()
{
    return objectCreationContextFromRendererContext(getFullRendererContext());
}

//TODO JS: Eventually, these should change per frame
//TODO JS: I think I would create the views up front, and then swap them in and out at bind time 
void vulkanRenderer::updateShadowImageViews(int frame, Scene* scene)
{
    int i = frame;
       
    rendererResources.shadowMapRenderingImageViews[i] =  MemoryArena::AllocSpan<VkImageView>(&rendererArena, MAX_SHADOWMAPS);
    rendererResources.WIP_shadowDepthPyramidInfos[i] = MemoryArena::AllocSpan<DepthPyramidInfo>(&rendererArena, MAX_SHADOWMAPS);
    for (int j = 0; j < MAX_SHADOWMAPS; j++)
    {
        rendererResources.shadowMapRenderingImageViews[i][j] = TextureUtilities::createImageViewCustomMip(
            getRendererForVkObjects(), rendererResources.shadowImages[i], shadowFormat,
            VK_IMAGE_ASPECT_DEPTH_BIT,
            (VkImageViewType)  VK_IMAGE_TYPE_2D,
            1, 
            j,1, 0);

        rendererResources.WIP_shadowDepthPyramidInfos[i][j] = static_createDepthPyramidResources(rendererVulkanObjects, &rendererArena, deletionQueue.get(), commandPoolmanager.get());
    }

    rendererResources.shadowMapSamplingImageViews[i] =  MemoryArena::AllocSpan<VkImageView>(&rendererArena, MAX_SHADOWCASTERS);
    for (int j = 0; j < MAX_SHADOWCASTERS; j++)
    {
        if (j < glm::min(scene->lightCount, MAX_SHADOWCASTERS))
        {
            VkImageViewType type = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
            int ct = shadowCountFromLightType(scene->lightTypes[j]);
            if ((lightType)scene->lightTypes[j] == LIGHT_POINT)
            {
                type = VK_IMAGE_VIEW_TYPE_CUBE;
            }
           
            rendererResources.shadowMapSamplingImageViews[i][j] = TextureUtilities::createImageViewCustomMip(
                getRendererForVkObjects(), rendererResources.shadowImages[i], shadowFormat,
                VK_IMAGE_ASPECT_DEPTH_BIT,
                (VkImageViewType)  type,
                ct, 
                scene->getShadowDataIndex(j), 1, 0);

         
        }
        else
        {
            rendererResources.shadowMapSamplingImageViews[i][j] = rendererResources.shadowMapRenderingImageViews[i][j];
        }
    }
}

RendererResources /*todo*/ static_initializeResources(rendererObjects initializedrenderer,  MemoryArena::memoryArena* allocationArena, RendererDeletionQueue* deletionQueue, CommandPoolManager* commandPoolmanager)
{
    std::vector<VkImage> swapchainImages = initializedrenderer.swapchain.get_images().value();
    std::vector<VkImageView> swapchainImageViews = initializedrenderer.swapchain.get_image_views().value();
    for(int i = 0; i < swapchainImageViews.size(); i++)
    {
        setDebugObjectName(initializedrenderer.vkbdevice.device, VK_OBJECT_TYPE_IMAGE, "Swapchain image", uint64_t(swapchainImages[i]));
        setDebugObjectName(initializedrenderer.vkbdevice.device, VK_OBJECT_TYPE_IMAGE_VIEW, "Swapchain image view", uint64_t(swapchainImageViews[i]));
    }

    for(int i = 0; i < swapchainImageViews.size(); i++)
    {
        deletionQueue->push_backVk(deletionType::ImageView, uint64_t(swapchainImageViews[i]));
    }

    //Create shadow image(s) 
    std::vector<VkImage> shadowImages = {};
    shadowImages.resize(MAX_FRAMES_IN_FLIGHT);
    std::vector<VkSampler> shadowSamplers = {};
    shadowSamplers.resize(MAX_FRAMES_IN_FLIGHT);
    std::vector<VkSampler> shadowSamplersWithMips = {};
    shadowSamplersWithMips.resize(MAX_FRAMES_IN_FLIGHT);
    std::vector<VmaAllocation> shadowMemory = {};
    shadowMemory.resize(MAX_FRAMES_IN_FLIGHT);
    
    auto shadowMapRenderingImageViews = MemoryArena::AllocSpan<std::span<VkImageView>>(allocationArena, MAX_FRAMES_IN_FLIGHT); 

    auto shadowMapSamplingImageViews = MemoryArena::AllocSpan<std::span<VkImageView>>(allocationArena, MAX_FRAMES_IN_FLIGHT);
    
    auto shadowMapDepthImageInfos = MemoryArena::AllocSpan<std::span<DepthPyramidInfo>>(allocationArena, MAX_FRAMES_IN_FLIGHT); 

    //Load shaders


    auto shaderLoader =  std::make_unique<ShaderLoader>(initializedrenderer.vkbdevice.device);
    shaderLoader->AddShader("triangle", L"./Shaders/Shader1_copy.hlsl");
    shaderLoader->AddShader("triangle_alt", L"./Shaders/shader1.hlsl");
    shaderLoader->AddShader("shadow", L"./Shaders/bindlessShadow.hlsl");
    shaderLoader->AddShader("lines", L"./Shaders/lines.hlsl");
    shaderLoader->AddShader("cull", L"./Shaders/cull_compute.hlsl", true);
    shaderLoader->AddShader("mipChain", L"./Shaders/mipchain_compute.hlsl", true);


    return {
        .shaderLoader = std::move(shaderLoader),
        .swapchainImages = swapchainImages, 
        .swapchainImageViews = swapchainImageViews,
        .shadowMapRenderingImageViews = (shadowMapRenderingImageViews),
        .shadowMapSamplingImageViews =  (shadowMapSamplingImageViews),
        .shadowSamplers = std::move(shadowSamplers),
        .shadowImages = std::move(shadowImages),
        .WIP_shadowDepthPyramidInfos = shadowMapDepthImageInfos,
        .shadowMemory = std::move(shadowMemory),
        .depthBufferInfo =  static_createDepthResources(initializedrenderer, allocationArena, deletionQueue, commandPoolmanager),
        .depthPyramidInfo =  static_createDepthPyramidResources(initializedrenderer, allocationArena, deletionQueue, commandPoolmanager),
    };
}


//TODO JS: break dependency on Scene -- add some kind of interface or something.
void vulkanRenderer::initializeRendererForScene(Scene* scene) //todo remaining initialization refactor
{
    //shadows
    perLightShadowData = MemoryArena::AllocSpan<std::span<PerShadowData>>(&rendererArena, scene->lightCount);
    for(int i = 0; i < MAX_FRAMES_IN_FLIGHT ; i++)
    {
        TextureUtilities::createImage(getRendererForVkObjects(),SHADOW_MAP_SIZE, SHADOW_MAP_SIZE,shadowFormat,VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
               VK_IMAGE_USAGE_SAMPLED_BIT,0,rendererResources.shadowImages[i],rendererResources.shadowMemory[i],1, MAX_SHADOWMAPS, true);
        createTextureSampler(&rendererResources.shadowSamplers[i], getFullRendererContext(), VK_SAMPLER_ADDRESS_MODE_REPEAT, 0, 1, true);
        setDebugObjectName(rendererVulkanObjects.vkbdevice.device, VK_OBJECT_TYPE_IMAGE, "SHADOW IMAGE", (uint64_t)(rendererResources.shadowImages[i]));
        setDebugObjectName(rendererVulkanObjects.vkbdevice.device, VK_OBJECT_TYPE_SAMPLER, "SHADOW IMAGE SAMPLER", (uint64_t)(rendererResources.shadowSamplers[i]));
        updateShadowImageViews(i, scene);
    }

    createDepthPyramidSampler(&rendererResources.depthMipSampler, getFullRendererContext(),HIZDEPTH);  

    //Initialize scene-ish objects we don't have a place for yet 
    cubemaplut_utilitytexture_index = AssetDataAndMemory->AddTexture(
        createTexture(getFullRendererContext(), "textures/outputLUT.png", TextureType::DATA_DONT_COMPRESS));
    cube_irradiance = createTexture(getFullRendererContext(), "textures/output_cubemap2_diff8.ktx2", TextureType::CUBE);
    cube_specular = createTexture(getFullRendererContext(), "textures/output_cubemap2_spec8.ktx2", TextureType::CUBE);

    createUniformBuffers(scene->objectsCount(),scene->lightCount);


    //TODO JS: Move... Run when meshes change?
    populateMeshBuffers();

    createDescriptorSetPool(getFullRendererContext(), &descriptorPool);

    Array opaqueLayout = MemoryArena::AllocSpan<VkDescriptorSetLayoutBinding>(&rendererArena, 12);
    uint32_t i =0;
    opaqueLayout.push_back({i++, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL, VK_NULL_HANDLE}); //Globals
    opaqueLayout.push_back({i++, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, MAX_TEXTURES, VK_SHADER_STAGE_FRAGMENT_BIT,  VK_NULL_HANDLE});
    opaqueLayout.push_back({i++, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,2, VK_SHADER_STAGE_FRAGMENT_BIT,  VK_NULL_HANDLE});
    opaqueLayout.push_back({i++, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, MAX_SHADOWMAPS, VK_SHADER_STAGE_FRAGMENT_BIT  }); //SHADOW
    opaqueLayout.push_back({i++, VK_DESCRIPTOR_TYPE_SAMPLER, MAX_TEXTURES, VK_SHADER_STAGE_FRAGMENT_BIT , VK_NULL_HANDLE} );
    opaqueLayout.push_back({i++, VK_DESCRIPTOR_TYPE_SAMPLER, 2, VK_SHADER_STAGE_FRAGMENT_BIT, VK_NULL_HANDLE}  );
    opaqueLayout.push_back({i++, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_NULL_HANDLE}  ); //SHADOW
    opaqueLayout.push_back({i++, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, VK_NULL_HANDLE} ); //Geometry
    opaqueLayout.push_back({i++, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,  VK_SHADER_STAGE_FRAGMENT_BIT, VK_NULL_HANDLE} ); //light
    opaqueLayout.push_back({i++, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,  VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, VK_NULL_HANDLE} ); //ubo
    opaqueLayout.push_back({i++, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, VK_NULL_HANDLE} ); //shadow matrices
    
    opaqueLayout.push_back({i++, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, VK_NULL_HANDLE} ); //vert positoins


    Array shadowLayout = MemoryArena::AllocSpan<VkDescriptorSetLayoutBinding>(&rendererArena, 5);
    // shadowLayout.push_back({6, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1, VK_SHADER_STAGE_VERTEX_BIT, VK_NULL_HANDLE});
    // shadowLayout.push_back({7, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1, VK_SHADER_STAGE_VERTEX_BIT, VK_NULL_HANDLE});  //Vertices
    shadowLayout.push_back({8, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1, VK_SHADER_STAGE_VERTEX_BIT, VK_NULL_HANDLE}); //light
    shadowLayout.push_back({9, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1, VK_SHADER_STAGE_VERTEX_BIT, VK_NULL_HANDLE}); //ubo
    // shadowLayout.push_back({10, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1, VK_SHADER_STAGE_VERTEX_BIT, VK_NULL_HANDLE}); //shadow matrices

    shadowLayout.push_back({11, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1, VK_SHADER_STAGE_VERTEX_BIT, VK_NULL_HANDLE}); //vert positoins


    Array cullingLayout = MemoryArena::AllocSpan<VkDescriptorSetLayoutBinding>(&rendererArena, 3);
    cullingLayout.push_back({0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1, VK_SHADER_STAGE_COMPUTE_BIT, VK_NULL_HANDLE}); //frustum data
    cullingLayout.push_back({1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1, VK_SHADER_STAGE_COMPUTE_BIT, VK_NULL_HANDLE}); //draws 
    cullingLayout.push_back({2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1, VK_SHADER_STAGE_COMPUTE_BIT, VK_NULL_HANDLE}); //objectData

    Array mipChainLayout = MemoryArena::AllocSpan<VkDescriptorSetLayoutBinding>(&rendererArena, 3);
    mipChainLayout.push_back({0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,1, VK_SHADER_STAGE_COMPUTE_BIT, VK_NULL_HANDLE}); //depth pyramid inout
    mipChainLayout.push_back({1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,1, VK_SHADER_STAGE_COMPUTE_BIT, VK_NULL_HANDLE}); //depth pyramid output
    mipChainLayout.push_back({2, VK_DESCRIPTOR_TYPE_SAMPLER,1, VK_SHADER_STAGE_COMPUTE_BIT, VK_NULL_HANDLE}); //depth pyramid inout

    for(int i = 0; i < MAX_FRAMES_IN_FLIGHT ; i++)
    {
        opaqueUpdates[i] = createOpaqueDescriptorUpdates(i, glm::min(MAX_SHADOWCASTERS, scene->lightCount), &rendererArena, opaqueLayout.getSpan());
        shadowUpdates[i] = MemoryArena::AllocSpan<std::span<descriptorUpdateData>>(&rendererArena, MAX_SHADOWCASTERS);
        for(int j = 0; j < MAX_SHADOWCASTERS; j++)
        {
            shadowUpdates[i][j] = createShadowDescriptorUpdates(&rendererArena, i, j, shadowLayout.getSpan());
            
        }
    }

    //TODO JS: Next validation errors point here, because I'm re-setting the descriptor sets repeatedly.\
    //TODO JS: Either need to refactor  comp shader to be bindless style or address immediate cause.
    descriptorsetLayoutsDataMipChain = PipelineGroup(getFullRendererContext(), descriptorPool, mipChainLayout.getSpan(), HIZDEPTH, "mip chain layout");
    setDebugObjectName(rendererVulkanObjects.vkbdevice.device, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,"mip chain descriptor layout", 
    uint64_t(descriptorsetLayoutsDataMipChain.pipelineData.perSceneDescriptorSetLayout));

    createGraphicsPipeline( rendererResources.shaderLoader->compiledShaders["mipChain"] ,  &descriptorsetLayoutsDataMipChain, {},true, sizeof(glm::vec2));

    descriptorsetLayoutsDataCulling = PipelineGroup(getFullRendererContext(), descriptorPool, cullingLayout.getSpan(), 1, "culling layout");
    setDebugObjectName(rendererVulkanObjects.vkbdevice.device, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,"cullingdescriptor layout", 
    uint64_t(descriptorsetLayoutsDataCulling.pipelineData.perSceneDescriptorSetLayout));


    
    //compute
    createGraphicsPipeline( rendererResources.shaderLoader->compiledShaders["cull"] ,  &descriptorsetLayoutsDataCulling, {},true, sizeof(cullPConstants));
    setDebugObjectName(rendererVulkanObjects.vkbdevice.device, VK_OBJECT_TYPE_PIPELINE,"cullt shader", uint64_t(descriptorsetLayoutsDataCulling.getPipeline(0)));
    
    descriptorsetLayoutsData = PipelineGroup(getFullRendererContext(), descriptorPool, opaqueLayout.getSpan(), 1,  "opaque layout");
    setDebugObjectName(rendererVulkanObjects.vkbdevice.device, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,"opaquedescriptor layout", uint64_t(descriptorsetLayoutsData.pipelineData.perSceneDescriptorSetLayout));
    descriptorsetLayoutsDataShadow = PipelineGroup(getFullRendererContext(), descriptorPool, shadowLayout.getSpan(), 1,  "shadow layout");
    setDebugObjectName(rendererVulkanObjects.vkbdevice.device, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,"shadowdescriptor layout", uint64_t(descriptorsetLayoutsDataShadow.pipelineData.perSceneDescriptorSetLayout));

   auto swapchainFormat = rendererVulkanObjects.swapchain.image_format;


    //opaque
    const PipelineGroup::graphicsPipelineSettings opaquePipelineSettings =  {.colorFormats = std::span(&swapchainFormat, 1),.depthFormat =  rendererResources.depthBufferInfo.format, .depthWriteEnable = VK_FALSE};
    createGraphicsPipeline( rendererResources.shaderLoader->compiledShaders["triangle_alt"],  &descriptorsetLayoutsData, opaquePipelineSettings,false, sizeof(debugLinePConstants));
    setDebugObjectName(rendererVulkanObjects.vkbdevice.device, VK_OBJECT_TYPE_PIPELINE,"triangle alt shader", uint64_t(descriptorsetLayoutsData.getPipeline(0)));
    createGraphicsPipeline( rendererResources.shaderLoader->compiledShaders["triangle"],  &descriptorsetLayoutsData, opaquePipelineSettings,false, sizeof(debugLinePConstants));
    setDebugObjectName(rendererVulkanObjects.vkbdevice.device, VK_OBJECT_TYPE_PIPELINE,"triangle  shader", uint64_t(descriptorsetLayoutsData.getPipeline(1)));
    const PipelineGroup::graphicsPipelineSettings linePipelineSettings =  {std::span(&swapchainFormat, 1), rendererResources.depthBufferInfo.format, VK_CULL_MODE_NONE, VK_PRIMITIVE_TOPOLOGY_LINE_LIST };
    createGraphicsPipeline( rendererResources.shaderLoader->compiledShaders["lines"],  &descriptorsetLayoutsData, linePipelineSettings,false, sizeof(debugLinePConstants));
    setDebugObjectName(rendererVulkanObjects.vkbdevice.device, VK_OBJECT_TYPE_PIPELINE,"lines  shader", uint64_t(descriptorsetLayoutsData.getPipeline(2)));

    //shadow 
    const PipelineGroup::graphicsPipelineSettings shadowPipelineSettings =  {std::span(&swapchainFormat, 0), shadowFormat, VK_CULL_MODE_NONE, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_TRUE, VK_FALSE, VK_TRUE, true };
    createGraphicsPipeline( rendererResources.shaderLoader->compiledShaders["shadow"],  &descriptorsetLayoutsDataShadow, shadowPipelineSettings,false, sizeof(shadowPushConstants));
    const PipelineGroup::graphicsPipelineSettings depthPipelineSettings =
        {std::span(&swapchainFormat, 0), shadowFormat, VK_CULL_MODE_NONE, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        VK_FALSE, VK_TRUE, VK_TRUE, true };

    const PipelineGroup::graphicsPipelineSettings depthPipelineSettings2 =
    {std::span(&swapchainFormat, 0), shadowFormat, VK_CULL_MODE_NONE, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    VK_FALSE, VK_TRUE, VK_FALSE, false };

    //TODO JS: need a better way tohandle these depth prepass shaders -- should have their own layout(s), should compile the vertex shader from the actual standard shader, and should group easily for lookup
    createGraphicsPipeline( rendererResources.shaderLoader->compiledShaders["shadow"],  &descriptorsetLayoutsDataShadow, depthPipelineSettings,false, sizeof(shadowPushConstants));
    std::span<VkPipelineShaderStageCreateInfo> triangleShaderSpan =  rendererResources.shaderLoader->compiledShaders["triangle"];
    createGraphicsPipeline( triangleShaderSpan.subspan(0, 1),  &descriptorsetLayoutsData, depthPipelineSettings2,false, sizeof(shadowPushConstants));
    setDebugObjectName(rendererVulkanObjects.vkbdevice.device, VK_OBJECT_TYPE_PIPELINE,"depth prepass shader", uint64_t(descriptorsetLayoutsData.getPipeline(3)));
    setDebugObjectName(rendererVulkanObjects.vkbdevice.device, VK_OBJECT_TYPE_PIPELINE,"depth prepass shader", uint64_t(descriptorsetLayoutsDataShadow.getPipeline(1)));
     for (auto kvp : rendererResources.shaderLoader->compiledShaders)
     {
         for(auto compiled_shader : kvp.second)
         {
             vkDestroyShaderModule(rendererVulkanObjects.vkbdevice.device, compiled_shader.module, nullptr);
         }
     }
    shaderGroups = {};
    Array opaqueShaderLookups = MemoryArena::AllocSpan<int>(&rendererArena, descriptorsetLayoutsData.getPipelineCt());
    Array shadowShaderLookups = MemoryArena::AllocSpan<int>(&rendererArena, descriptorsetLayoutsData.getPipelineCt());
    Array cullShaderLookups = MemoryArena::AllocSpan<int>(&rendererArena, descriptorsetLayoutsData.getPipelineCt());
    for(int i = 0; i < descriptorsetLayoutsData.getPipelineCt(); i++)
    {
        opaqueShaderLookups.push_back( {i});
                  shadowShaderLookups.push_back({0});
                  cullShaderLookups.push_back({0});
    }
    shaderGroups = {
        .opaqueShaders = {opaqueShaderLookups.getSpan(), &descriptorsetLayoutsData},
    .shadowShaders = {shadowShaderLookups.getSpan(), &descriptorsetLayoutsDataShadow},
    .cullShaders = {cullShaderLookups.getSpan(), &descriptorsetLayoutsDataCulling}
    };

}

//TODO JS: Support changing meshes at runtime
void vulkanRenderer::populateMeshBuffers()
{
    size_t indexCt = 0;
    size_t vertCt = 0;
    for (int i = 0; i < AssetDataAndMemory->meshCount; i++)
    {
        indexCt += AssetDataAndMemory->backing_meshes[i].indices.size();
        vertCt += AssetDataAndMemory->backing_meshes[i].vertices.size();
    }

    auto gpuVerts = MemoryArena::AllocSpan<gpuvertex>(getFullRendererContext().arena, vertCt);
    auto Positoins = MemoryArena::AllocSpan<glm::vec4>(getFullRendererContext().arena, vertCt);
    auto Indices = MemoryArena::AllocSpan<uint32_t>(getFullRendererContext().arena, indexCt);
    size_t vert = 0;
    size_t _vert = 0;
    size_t meshoffset = 0;
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
        meshoffset += mesh.vertices.size();
    }

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        FramesInFlightData[i].hostMesh.updateMappedMemory({gpuVerts.data(),vertCt});
        FramesInFlightData[i].hostVerts.updateMappedMemory({Positoins.data(),vertCt});
        FramesInFlightData[i].hostIndices.updateMappedMemory({Indices.data(), AssetDataAndMemory->getIndexCount()});
    }
}

//TODO JS: https://gpuopen-librariesandsdks.github.io/VulkanMemoryAllocator/html/usage_patterns.html advanced
void vulkanRenderer::createUniformBuffers(int objectsCount, int lightCount)
{
    VkDeviceSize globalsSize = sizeof(ShaderGlobals);
    VkDeviceSize ubosSize = sizeof(UniformBufferObject) * objectsCount;
    VkDeviceSize vertsSize = sizeof(gpuvertex) * AssetDataAndMemory->getIndexCount();
    VkDeviceSize lightdataSize = sizeof(gpulight) *lightCount;
    VkDeviceSize shadowDataSize = sizeof(PerShadowData) *lightCount * 10; //times six is plenty right?

    RendererContext context = getFullRendererContext();

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        FramesInFlightData[i].perLightShadowShaderGlobalsBuffer.resize(MAX_SHADOWCASTERS);

        for (size_t j = 0; j < MAX_SHADOWCASTERS ; j++)
        {
            FramesInFlightData[i].perLightShadowShaderGlobalsBuffer[j] = createDataBuffer<ShaderGlobals>(&context, 1, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
        }

       

        
        FramesInFlightData[i].opaqueShaderGlobalsBuffer = createDataBuffer<ShaderGlobals>(&context, 1, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
        FramesInFlightData[i].uniformBuffers = createDataBuffer<UniformBufferObject>(&context, objectsCount, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT); //
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
void vulkanRenderer::createDescriptorSetPool(RendererContext context, VkDescriptorPool* pool)
{
    
    std::vector<VkDescriptorPoolSize> sizes =
    {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 20 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 20 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 20 },
        { VK_DESCRIPTOR_TYPE_SAMPLER, 20 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 20 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 20 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 20 }
    };

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = 0;
    pool_info.poolSizeCount = (uint32_t)sizes.size();
    pool_info.pPoolSizes = sizes.data();
    
    pool_info.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT) * 4 * 3; //4 here is the number of descriptorsets  * 3 pipelines
    VK_CHECK( vkCreateDescriptorPool(context.device, &pool_info, nullptr, pool));
    context.rendererdeletionqueue->push_backVk(deletionType::descriptorPool,uint64_t(*pool));
    
}



void vulkanRenderer::updateOpaqueDescriptorSets(PipelineGroup* layoutData)
{
    layoutData->updateDescriptorSets(opaqueUpdates[currentFrame], currentFrame, 0);
}



std::span<descriptorUpdateData> vulkanRenderer::createOpaqueDescriptorUpdates(uint32_t frame, int shadowCasterCount, MemoryArena::memoryArena* arena, std::span<VkDescriptorSetLayoutBinding> layoutBindings)
{

 
    auto imageInfos = AssetDataAndMemory->textures.getSpan();
    std::span<VkDescriptorImageInfo> cubeImageInfos = MemoryArena::AllocSpan<VkDescriptorImageInfo>(arena, 2);
    cubeImageInfos[0] = cube_irradiance.vkImageInfo;
    cubeImageInfos[1] = cube_specular.vkImageInfo;


    int pointCount = 0;

    std::span<VkDescriptorImageInfo> shadows = MemoryArena::AllocSpan<VkDescriptorImageInfo>(arena, MAX_SHADOWMAPS);
    for(int i = 0; i < MAX_SHADOWMAPS; i++)
    {
        VkImageView view {};
        if (i < shadowCasterCount)
        {
            view =  rendererResources.shadowMapSamplingImageViews[frame][i] ;
        }

        //Fill out the remaining leftover shadow bindings with something -- using the other shadow view is arbitrary
        //TODO JS: Should have a "default" view maybe? 
        else
        {
            view = rendererResources.shadowMapRenderingImageViews[frame][0];
        }
        
        assert( &view != VK_NULL_HANDLE);
        VkDescriptorImageInfo shadowInfo = {
            .imageView = view, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };
        shadows[i] = shadowInfo;
    }

    VkDescriptorImageInfo* shadowSamplerInfo =  MemoryArena::Alloc<VkDescriptorImageInfo>(arena);
    *shadowSamplerInfo = {
        .sampler  = rendererResources.shadowSamplers[frame], .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };
    
    // VkDescriptorBufferInfo* meshBufferinfo = MemoryArena::Alloc<VkDescriptorBufferInfo>(arena); 
    // *meshBufferinfo = FramesInFlightData[frame].meshBuffers.buffer.getBufferInfo();
    VkDescriptorBufferInfo* lightbufferinfo = MemoryArena::Alloc<VkDescriptorBufferInfo>(arena); 
    *lightbufferinfo = FramesInFlightData[frame].lightBuffers.buffer.getBufferInfo();
    VkDescriptorBufferInfo* uniformbufferinfo = MemoryArena::Alloc<VkDescriptorBufferInfo>(arena); 
    *uniformbufferinfo = FramesInFlightData[frame].uniformBuffers.buffer.getBufferInfo();
    VkDescriptorBufferInfo* shaderglobalsinfo =  MemoryArena::Alloc<VkDescriptorBufferInfo>(arena);
    *shaderglobalsinfo = FramesInFlightData[frame].opaqueShaderGlobalsBuffer.buffer.getBufferInfo();
    VkDescriptorBufferInfo* shadowBuffersInfo = MemoryArena::Alloc<VkDescriptorBufferInfo>(arena); 
    *shadowBuffersInfo = FramesInFlightData[frame].shadowDataBuffers.buffer.getBufferInfo();
    
    VkDescriptorBufferInfo* vertBufferinfo = MemoryArena::Alloc<VkDescriptorBufferInfo>(arena); 
    *vertBufferinfo = FramesInFlightData[frame].deviceVerts.getBufferInfo();


    VkDescriptorBufferInfo* meshBufferinfo = MemoryArena::Alloc<VkDescriptorBufferInfo>(arena); 
    *meshBufferinfo = FramesInFlightData[frame].deviceMesh.getBufferInfo();


    Array<descriptorUpdateData> descriptorUpdates = MemoryArena::AllocSpan<descriptorUpdateData>(arena, 12);
    //Update descriptor sets with data
    descriptorUpdates.push_back({VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, shaderglobalsinfo});

    descriptorUpdates.push_back({VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, imageInfos.data(),  (uint32_t)imageInfos.size()});
    descriptorUpdates.push_back({VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, cubeImageInfos.data(), (uint32_t)cubeImageInfos.size()});
    descriptorUpdates.push_back({VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, shadows.data(),  MAX_SHADOWMAPS}); //shadows

    descriptorUpdates.push_back({VK_DESCRIPTOR_TYPE_SAMPLER, imageInfos.data(), (uint32_t)imageInfos.size()});
    descriptorUpdates.push_back({VK_DESCRIPTOR_TYPE_SAMPLER, cubeImageInfos.data(), (uint32_t)cubeImageInfos.size()});
    descriptorUpdates.push_back({VK_DESCRIPTOR_TYPE_SAMPLER, shadowSamplerInfo, (uint32_t)1}); //shadows

    descriptorUpdates.push_back({VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, meshBufferinfo});
    descriptorUpdates.push_back({VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, lightbufferinfo});
    descriptorUpdates.push_back({VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, uniformbufferinfo});
    
    descriptorUpdates.push_back({VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, shadowBuffersInfo});

    descriptorUpdates.push_back({VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, vertBufferinfo});

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

std::span<descriptorUpdateData> vulkanRenderer::createShadowDescriptorUpdates(MemoryArena::memoryArena* arena, uint32_t frame, uint32_t shadowIndex,  std::span<VkDescriptorSetLayoutBinding> layoutBindings)
{
    //Get data
    // VkDescriptorBufferInfo* meshBufferinfo = MemoryArena::Alloc<VkDescriptorBufferInfo>(arena); 
    // *meshBufferinfo = getDescriptorBufferInfo(FramesInFlightData[frame].meshBuffers);
    VkDescriptorBufferInfo* uniformbufferinfo = MemoryArena::Alloc<VkDescriptorBufferInfo>(arena); 
    *uniformbufferinfo = getDescriptorBufferInfo(FramesInFlightData[frame].uniformBuffers);
    // VkDescriptorBufferInfo* shaderglobalsinfo = MemoryArena::Alloc<VkDescriptorBufferInfo>(arena); 
    // *shaderglobalsinfo = getDescriptorBufferInfo(FramesInFlightData[frame].perLightShadowShaderGlobalsBuffer[shadowIndex]);

    VkDescriptorBufferInfo* lightbufferinfo = MemoryArena::Alloc<VkDescriptorBufferInfo>(arena); 
    *lightbufferinfo = getDescriptorBufferInfo(FramesInFlightData[frame].lightBuffers);
    // VkDescriptorBufferInfo* shadowBuffersInfo = MemoryArena::Alloc<VkDescriptorBufferInfo>(arena); 
    // *shadowBuffersInfo = getDescriptorBufferInfo(FramesInFlightData[frame].shadowDataBuffers);
    
    VkDescriptorBufferInfo* vertBufferinfo = MemoryArena::Alloc<VkDescriptorBufferInfo>(arena); 
    *vertBufferinfo = FramesInFlightData[frame].deviceVerts.getBufferInfo();
    
     // VkDescriptorBufferInfo* meshBufferinfo = MemoryArena::Alloc<VkDescriptorBufferInfo>(arena); 
     // *meshBufferinfo = FramesInFlightData[frame].deviceMesh.getBufferInfo();


    Array<descriptorUpdateData> descriptorUpdates = MemoryArena::AllocSpan<descriptorUpdateData>(arena, 5);
    //Update descriptor sets with data
 
    // descriptorUpdates.push_back({VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, shaderglobalsinfo});
    // descriptorUpdates.push_back({VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, meshBufferinfo});
    descriptorUpdates.push_back({VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, lightbufferinfo});
    descriptorUpdates.push_back({VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, uniformbufferinfo});
    // descriptorUpdates.push_back({VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, shadowBuffersInfo});


    descriptorUpdates.push_back({VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, vertBufferinfo});
    
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
void vulkanRenderer::updateShadowDescriptorSets(PipelineGroup* layoutData, uint32_t shadowIndex)
{
    layoutData->
    updateDescriptorSets(shadowUpdates[currentFrame][shadowIndex], currentFrame, 0);

    
}
#pragma endregion

void createSemaphore(VkDevice device,   VkSemaphore* semaphorePtr, const char* debugName, RendererDeletionQueue* deletionQueue)
{
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VK_CHECK(vkCreateSemaphore(device, &semaphoreInfo, nullptr, semaphorePtr));
    deletionQueue->push_backVk(deletionType::Semaphore,uint64_t(*semaphorePtr));
    setDebugObjectName(device, VK_OBJECT_TYPE_SEMAPHORE, debugName, uint64_t(*semaphorePtr));
    
}
void static_createSyncObjects(VkDevice device,  RendererDeletionQueue* deletionQueue, acquireImageSemaphore* semaphoresToCreate, VkFence* fence)
{

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        createSemaphore(device,  &semaphoresToCreate->semaphore, "imageAvailableSemaphores", deletionQueue);
        // createSemaphore(device,  &semaphoresToCreate->renderFinishedSemaphores, "renderFinishedSemaphores", deletionQueue);
    //
    // createSemaphore(device,  &semaphoresToCreate->depthPrepassFinishedSemaphore, "depthPrepassFinishedSemaphore", deletionQueue);
    //     createSemaphore(device,  &semaphoresToCreate->depthPrepassMipsFinishedSempahore, "depthPrepassMipsFinishedSempahore", deletionQueue);
    //
    //     createSemaphore(device,  &semaphoresToCreate->computeFinishedSemaphores, "computeFinishedSemaphores", deletionQueue);
    //     
    //     createSemaphore(device,  &semaphoresToCreate->shadowAvailableSemaphores, "shadowAvailableSemaphores", deletionQueue);
    //     createSemaphore(device,  &semaphoresToCreate->shadowFinishedSemaphores, "shadowFinishedSemaphores", deletionQueue);
    //
    //     createSemaphore(device,  &semaphoresToCreate->swapchaintransitionedOutSemaphores, "swapchaintransitionedOutSemaphores", deletionQueue);
    //     createSemaphore(device,  &semaphoresToCreate->swapchaintransitionedInSemaphores, "swapchaintransitionedInSemaphores", deletionQueue);
    //  
    //
    //     createSemaphore(device,  &semaphoresToCreate->shadowtransitionedOutSemaphores, "shadowtransitionedOutSemaphores", deletionQueue);
    //     createSemaphore(device,  &semaphoresToCreate->shadowtransitionedInSemaphores, "shadowtransitionedInSemaphores", deletionQueue);

        VK_CHECK(vkCreateFence(device, &fenceInfo, nullptr, fence));
        deletionQueue->push_backVk(deletionType::Fence, uint64_t(*fence));
   
   

}

//TODO JS: none of this belongs here except for actually submitting the updates
#pragma region per-frame updates 

Transform getCameraTransform(cameraData camera)
{
    
    Transform output{};
    output.translation = glm::translate(glm::mat4(1.0), -camera.eyePos);
    output.rot = glm::mat4_cast(glm::conjugate(OrientationFromYawPitch(camera.eyeRotation)));

    return output;
}
struct linePair
{
    glm::vec3 start;
    glm::vec3 end;
    glm::vec3 color;
};

//zoux code
VkBufferMemoryBarrier2 bufferBarrier(VkBuffer buffer, VkPipelineStageFlags2 srcStageMask, VkAccessFlags2 srcAccessMask, VkPipelineStageFlags2 dstStageMask, VkAccessFlags2 dstAccessMask)
{
    VkBufferMemoryBarrier2 result = { VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2 };

    result.srcStageMask = srcStageMask;
    result.srcAccessMask = srcAccessMask;
    result.dstStageMask = dstStageMask;
    result.dstAccessMask = dstAccessMask;
    result.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    result.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    result.buffer = buffer;
    result.offset = 0;
    result.size = VK_WHOLE_SIZE;

    return result;
}
//zoux code
void pipelineBarrier(VkCommandBuffer commandBuffer, VkDependencyFlags dependencyFlags, size_t bufferBarrierCount, const VkBufferMemoryBarrier2* bufferBarriers, size_t imageBarrierCount, const VkImageMemoryBarrier2* imageBarriers)
{
    VkDependencyInfo dependencyInfo = { VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
    dependencyInfo.dependencyFlags = dependencyFlags;
    dependencyInfo.bufferMemoryBarrierCount = unsigned(bufferBarrierCount);
    dependencyInfo.pBufferMemoryBarriers = bufferBarriers;
    dependencyInfo.imageMemoryBarrierCount = unsigned(imageBarrierCount);
    dependencyInfo.pImageMemoryBarriers = imageBarriers;

    vkCmdPipelineBarrier2(commandBuffer, &dependencyInfo);
}
//zoux code
VkImageMemoryBarrier2 imageBarrier(VkImage image, VkPipelineStageFlags2 srcStageMask, VkAccessFlags2 srcAccessMask, VkImageLayout oldLayout, VkPipelineStageFlags2 dstStageMask, VkAccessFlags2 dstAccessMask, VkImageLayout newLayout, VkImageAspectFlags aspectMask, uint32_t baseMipLevel, uint32_t levelCount)
{
    VkImageMemoryBarrier2 result = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };

    result.srcStageMask = srcStageMask;
    result.srcAccessMask = srcAccessMask;
    result.dstStageMask = dstStageMask;
    result.dstAccessMask = dstAccessMask;
    result.oldLayout = oldLayout;
    result.newLayout = newLayout;
    result.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    result.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    result.image = image;
    result.subresourceRange.aspectMask = aspectMask;
    result.subresourceRange.baseMipLevel = baseMipLevel;
    result.subresourceRange.levelCount = levelCount;
    result.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

    return result;
}

std::vector<linePair> debugLines;

void debugDrawFrustum(std::span<glm::vec4> frustum)
{
    //Back plane
    debugLines.push_back({{frustum[0]},{frustum[1]}, {1,0,0}});
    debugLines.push_back({{frustum[1]},{frustum[2]}, {1,0,0}});
    debugLines.push_back({{frustum[2]},{frustum[3]}, {1,0,0}});
    debugLines.push_back({{frustum[3]},{frustum[0]}, {1,0,0}});

    //front plane 
    debugLines.push_back({{frustum[4 + 0]},{frustum[4 + 1]}, {0.5,0,0}});
    debugLines.push_back({{frustum[4 + 1]},{frustum[4 + 2]}, {0.5,0,0}});
    debugLines.push_back({{frustum[4 + 2]},{frustum[4 + 3]}, {0.5,0,0}});
    debugLines.push_back({{frustum[4 + 3]},{frustum[4 + 0]}, {0.5,0,0}});

    //sides 
    debugLines.push_back({{frustum[0]},{frustum[4 + 0]}, {1,1,1}});
    debugLines.push_back({{frustum[1]},{frustum[4 + 1]}, {1,1,1}});
    debugLines.push_back({{frustum[2]},{frustum[4 + 2]}, {1,1,1}});
    debugLines.push_back({{frustum[3]},{frustum[4 + 3]}, {1,1,1}});

}

void debugDrawCross(glm::vec3 point, float size, glm::vec3 color)
{
    glm::vec3 xoffset = (glm::vec3{1,0,0} * (size / 2));
    glm::vec3 yoffset = (glm::vec3{0,1,0} * (size / 2));
    glm::vec3 zoffset = (glm::vec3{0,0,1} * (size / 2));
    debugLines.push_back({point - xoffset, point + xoffset, color});
    debugLines.push_back({point - yoffset, point + yoffset, color});
    debugLines.push_back({point - zoffset, point + zoffset, color});
}

glm::vec4 maxComponentsFromSpan(std::span<glm::vec4> input)
{

    float maxX = std::numeric_limits<float>::lowest();
    float maxY = std::numeric_limits<float>::lowest();
    float maxZ = std::numeric_limits<float>::lowest();
    float maxW = std::numeric_limits<float>::lowest();
    for (int i = 0; i < 8; i++)
    {
        auto vec = input[i];
        maxX = std::max<float>(maxX, vec.x);
        maxY = std::max<float>(maxY, vec.y);
        maxZ = std::max<float>(maxZ, vec.z);
        maxW = std::max<float>(maxW, vec.a);
    }

    return glm::vec4(maxX,maxY,maxZ,maxW);
    
}


glm::vec4 minComponentsFromSpan(std::span<glm::vec4> input)
{

    float minX = std::numeric_limits<float>::infinity();
    float minY = std::numeric_limits<float>::infinity();
    float minZ = std::numeric_limits<float>::infinity();
    float minW = std::numeric_limits<float>::infinity();
    for (int i = 0; i < 8; i++)
    {
        auto vec = input[i];
        minX = std::min<float>(minX, vec.x);
        minY = std::min<float>(minY, vec.y);
        minZ = std::min<float>(minZ, vec.z);
        minW = std::min<float>(minW, vec.a);
    }

    return glm::vec4(minX,minY,minZ,minW);
    
}


std::span<glm::vec4> populateFrustumCornersForSpace(std::span<glm::vec4> output_span, glm::mat4 matrix)
{
   const glm::vec3 frustumCorners[8] = {
        glm::vec3( 1.0f, -1.0f, 0.0f), // bot right
        glm::vec3(-1.0f, -1.0f, 0.0f), //bot left
        glm::vec3(-1.0f,  1.0f, 0.0f), //top rgiht 
        glm::vec3( 1.0f,  1.0f, 0.0f), // top left
        glm::vec3( 1.0f, -1.0f,  1.0f), //bot r8ghyt  (far)
        glm::vec3(-1.0f, -1.0f,  1.0f), //bot left (far)
        glm::vec3(-1.0f,  1.0f,  1.0f), //top left (far)
        glm::vec3( 1.0f,  1.0f,  1.0f), //top right (far)
    };

    assert (output_span.size() == 8);
    for(int j = 0; j < 8; j++)
    {
        glm::vec4 invCorner =  matrix * glm::vec4(frustumCorners[j], 1.0f);
        output_span[j] =  invCorner /  (invCorner.w ) ;
    }

    return output_span;

}


viewProj viewProjFromCamera( cameraData camera)
{
    Transform cameraTform = getCameraTransform(camera);
    glm::mat4 view = cameraTform.rot * cameraTform.translation;

    glm::mat4 proj = glm::perspective(glm::radians(camera.fov),
                                      camera.extent.width / static_cast<float>(camera.extent.height),
                                      camera.nearPlane,
                                      camera.farPlane);
    proj[1][1] *= -1;

    return {view, proj};
}

std::span<PerShadowData> calculateLightMatrix(MemoryArena::memoryArena* allocator, cameraData cam, glm::vec3 lightPos, glm::vec3 spotDir, float spotRadius, lightType type)
{
    viewProj vp = viewProjFromCamera(cam);
    
    glm::vec3 dir = type ==  LIGHT_SPOT ? spotDir : glm::normalize(lightPos);
    glm::vec3 up = glm::vec3( 0.0f, 1.0f,  0.0f);
    //offset directions that are invalid for lookat
    if (abs(up) == abs(dir))
    {
        dir += glm::vec3(0.00001F);
        dir = glm::normalize(dir);
    }
    glm::mat4 lightViewMatrix = {};
    glm::mat4 lightProjection = {};
    debugDrawCross(lightPos, 2, {1,1,0});
    std::span<PerShadowData> outputSpan;
    switch (type)
    {
      
    case LIGHT_DIR:
        {

            outputSpan = MemoryArena::AllocSpan<PerShadowData>(allocator, CASCADE_CT ); 

            glm::mat4 invCam = glm::inverse(vp.proj * vp.view);

            glm::vec4 frustumCornersWorldSpace[8] = {};
            populateFrustumCornersForSpace(frustumCornersWorldSpace, invCam);

         
        
            debugDrawFrustum(frustumCornersWorldSpace);

            float clipRange = cam.farPlane - cam.nearPlane;
            float minZ =  cam.nearPlane;
            float maxZ =  cam.nearPlane + clipRange;

            float range = maxZ - minZ;
            float ratio = maxZ / minZ;

            //cascades
            float cascadeSplits[CASCADE_CT] = {};
            for (uint32_t i = 0; i < CASCADE_CT; i++) {
                float p = (i + 1) / static_cast<float>(CASCADE_CT);
                float log = minZ * std::pow(ratio, p);
                float uniform = minZ + range * p;
                float d = 0.92f * (log - uniform) + uniform;
                cascadeSplits[i] = (d - cam.nearPlane) / clipRange;
            }

            for (int i = 0; i < CASCADE_CT; i++)
            {
                float splitDist = cascadeSplits[i];
                float lastSplitDist = i == 0 ? 0 : cascadeSplits[i-1]; 
                
                glm::vec3 frustumCornersWorldSpacev3[8] = {}; //TODO JS: is this named correctly?
                for(int j = 0; j < 8; j++)
                {
                    frustumCornersWorldSpacev3[j] = glm::vec3(frustumCornersWorldSpace[j]);
                }
                for(int j = 0; j < 4; j++)
                {
                    glm::vec3 dist = frustumCornersWorldSpacev3[j + 4] - frustumCornersWorldSpacev3[j];
                    frustumCornersWorldSpacev3[j + 4] = frustumCornersWorldSpacev3[j] + (dist * splitDist);
                    frustumCornersWorldSpacev3[j] = frustumCornersWorldSpacev3[j] + (dist * lastSplitDist);
                }

                glm::vec3 frustumCenter = glm::vec3(0.0f);
                for (uint32_t j = 0; j < 8; j++) {
                    frustumCenter += frustumCornersWorldSpacev3[j];
                }
                frustumCenter /= 8.0f;

                debugDrawCross(frustumCenter, 20, {0,0,1});
               

   
                float radius = 0.0f;
                for (uint32_t i = 0; i < 8; i++) {
                    float distance = glm::length(glm::vec3(frustumCornersWorldSpacev3[i]) - frustumCenter);
                    radius = glm::max<float>(radius, distance);
                }
                
                radius = std::ceil(radius * 16.0f) / 16.0f;
                float offset = 12.0;


                //Texel clamping
                glm::mat4 texelScalar = glm::mat4(1);
                texelScalar = glm::scale(texelScalar, glm::vec3((float)SHADOW_MAP_SIZE / (radius * 2.0f)));
                glm::vec3 maxExtents = glm::vec3(radius);
                glm::vec3 minExtents = -maxExtents;
                glm::mat4 baseLightvew = glm::lookAt(normalize(-dir), glm::vec3(0),  up);
                glm::mat4 texelLightview = texelScalar * baseLightvew ;
                glm::mat4 texelInverse = (inverse(texelLightview));
                glm::vec4 transformedCenter = texelLightview * glm::vec4(frustumCenter, 1.0);
                transformedCenter.x = floor(transformedCenter.x);
                transformedCenter.y = floor(transformedCenter.y);
                transformedCenter = texelInverse * transformedCenter ;


                //Compute output matrices
                lightViewMatrix = glm::lookAt(glm::vec3(transformedCenter)  + ((dir * maxExtents) * offset), glm::vec3(transformedCenter), up);
                glm::mat4 lightOrthoMatrix = glm::ortho(minExtents.x, maxExtents.x, minExtents.y, maxExtents.y, 0.0f, (maxExtents.z - minExtents.z) * offset);

                lightProjection =  lightOrthoMatrix;
                outputSpan[i] = {lightViewMatrix, lightProjection,  (cam.nearPlane + splitDist * clipRange) * -1.0f};
            }
            return  outputSpan.subspan(0,CASCADE_CT);
        }
    case LIGHT_SPOT:
        {
            outputSpan = MemoryArena::AllocSpan<PerShadowData>(allocator, 1 ); 
            lightViewMatrix = glm::lookAt(lightPos, lightPos + dir, up);
            
            lightProjection = glm::perspective(glm::radians(spotRadius),
                                      1.0f, 0.1f,
                                      50.0f); //TODO BETTER FAR 
            outputSpan[0] = {lightViewMatrix, lightProjection,  0};
                                  
            return  outputSpan;
        }
    case LIGHT_POINT:

           {
        outputSpan = MemoryArena::AllocSpan<PerShadowData>(allocator, 6 ); 
        lightProjection = glm::perspective(glm::radians((float)90),
                                  1.0f, 0.001f,
                                  10.0f);} //TODO BETTER FAR

        for(int i = 0; i < outputSpan.size(); i++)
        {
            outputSpan[i].cascadeDepth = 0;
        }
        glm::mat4 translation = glm::translate(glm::mat4(1.0f), -lightPos);
        glm::mat4 rotMatrix = glm::mat4(1.0);
    
    outputSpan[0].view =  glm::rotate(rotMatrix, glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    outputSpan[0].view =  (glm::rotate(  outputSpan[0].view, glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)) * translation);
      rotMatrix = glm::mat4(1.0);
    outputSpan[1].view = glm::rotate(rotMatrix, glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    outputSpan[1].view =  (glm::rotate(outputSpan[1].view, glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)) * translation);
      rotMatrix = glm::mat4(1.0);
    outputSpan[2].view = (glm::rotate(rotMatrix, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f)) * translation);
      rotMatrix = glm::mat4(1.0);
    outputSpan[3].view = (glm::rotate(rotMatrix, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)) * translation);
      rotMatrix = glm::mat4(1.0);
    outputSpan[4].view = (glm::rotate(rotMatrix, glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)) * translation);
      rotMatrix = glm::mat4(1.0);
    outputSpan[5].view = (glm::rotate(rotMatrix, glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f)) * translation);
       rotMatrix = glm::mat4(1.0);

        for(int i =0; i < 6; i++)
        {
            outputSpan[i].proj = lightProjection;
        }
        return  outputSpan;
        }

        assert(!outputSpan.empty());
        return outputSpan;

}


void updateGlobals(cameraData camera, int lightCount, int cubeMapLutIndex, HostDataBufferObject<ShaderGlobals> globalsBuffer)
{
    ShaderGlobals globals{};
    viewProj vp = viewProjFromCamera(camera);
    globals.view = vp.view;
    globals.proj = vp.proj;
    globals.viewPos = glm::vec4(camera.eyePos.x, camera.eyePos.y, camera.eyePos.z, 1);
    globals.lightcountx_modey_shadowcountz_padding_w = glm::vec4(lightCount, debug_shader_bool_1, MAX_SHADOWCASTERS, 0);
    globals.cubemaplutidx_cubemaplutsampleridx_paddingzw = glm::vec4(
        cubeMapLutIndex,
        cubeMapLutIndex, 0, 0);
    
    globalsBuffer.updateMappedMemory({&globals, 1});

}

void updateShadowData(MemoryArena::memoryArena* allocator, std::span<std::span<PerShadowData>> perLightShadowData, Scene* scene, cameraData camera)
{
    for(int i =0; i <scene->lightCount; i++)
    {
        perLightShadowData[i] =  calculateLightMatrix(allocator, camera,
             (scene->lightposandradius[i]), scene->lightDir[i], scene->lightposandradius[i].w, static_cast<lightType>(scene->lightTypes[i]));
    }
}

//from niagara
glm::vec4 normalizePlane(glm::vec4 p)
{
    return p / length(glm::vec3(p));
}

void vulkanRenderer::updatePerFrameBuffers(uint32_t currentFrame, Array<std::span<glm::mat4>> models, Scene* scene)
{
    //TODO JS: to ring buffer?
    auto tempArena = getFullRendererContext().tempArena;

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
            viewProj vp = viewProjFromCamera(scene->sceneCamera);
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
    viewProj vp = viewProjFromCamera(scene->sceneCamera);
    glm::mat4  projT =   transpose(vp.proj);
    frustums[offset + 0] = projT[3] + projT[0];
    frustums[offset + 1] = projT[3] - projT[0];
    frustums[offset + 2] = projT[3] + projT[1];
    frustums[offset + 3] = projT[3] - projT[1];
    frustums[offset + 4] = glm::vec4();
    frustums[offset + 5] = glm::vec4();
    

    //Ubos
    auto ubos = MemoryArena::AllocSpan<UniformBufferObject>(tempArena,scene->objectsCount());

    for (int i = 0; i < ubos.size(); i++)
    {
        auto lookup = scene->transforms._transform_lookup[i];
        glm::mat4* model = &models[lookup.depth][lookup.index];
        ubos[i].model = *model;
        ubos[i].Normal = transpose(inverse(glm::mat3(*model)));


        
        // int i = drawIndices[j];
        Material material = AssetDataAndMemory->materials[scene->objects.materials[i]];
        
        ubos[i].props.indexInfo = glm::vec4(material.diffuseIndex, (AssetDataAndMemory->getOffsetFromMeshID(scene->objects.meshIndices[i])),
                                       material.diffuseIndex, 44);

        ubos[i].props.textureInfo = glm::vec4(material.diffuseIndex, material.specIndex, material.normalIndex, -1.0);

        ubos[i].props.materialprops = glm::vec4(material.roughness, material.roughness, 0, 0);
        ubos[i].props.color = glm::vec4(material.color,1.0f);

        positionRadius objectBounds = AssetDataAndMemory->meshBoundingSphereRad[scene->objects.meshIndices[i]];
        objectBounds.objectSpaceRadius *= glm::max(glm::max(scene->objects.scales[i].x, scene->objects.scales[i].y), scene->objects.scales[i].z); //TODO JS move earlier in the frame
        ubos[i].cullingInfo = objectBounds;
    }
    
    FramesInFlightData[currentFrame].uniformBuffers.updateMappedMemory({ubos.data(), (size_t)scene->objectsCount()});



}

void endCommandBufferRecording(ActiveRenderStepData* context)
{
    assert(context->active);
    VK_CHECK(vkEndCommandBuffer(context->commandBuffer));
    context->active = false;
}

VkRenderingAttachmentInfoKHR createRenderingAttatchmentWithLayout(VkImageView target, float clearColor, bool clear, VkImageLayout layout)
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
VkRenderingAttachmentInfoKHR createRenderingAttatchmentStruct(VkImageView target, float clearColor, bool clear)
{
   return createRenderingAttatchmentWithLayout(target, clearColor, clear,VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR);
}
void recordCommandBufferCulling(ActiveRenderStepData commandBufferContext, uint32_t currentFrame, ComputeCullListInfo pass,  VkBuffer indirectCommandsBuffer)
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

#pragma endregion
#pragma region draw
void recordPrimaryRenderPasses( std::span<RenderPassConfig> passes, VkBuffer indirectCommandsBuffer /*FramesInFlightData[currentFrame].drawBuffers._buffer.data*/, int currentFrame)
{
  
    uint32_t offset_into_struct = offsetof(drawCommandData, command);

    //Cache these at a higher level -- maybe per command buffer/when the command buffer is set? Rn these get invalidated every shadow draw, since they're all new renderpasses 
    
    for(int j = 0; j < passes.size(); j++)
    {
        //TODO recordDepthPrepass( std::span<RenderPassConfig> passes, VkBuffer indirectCommandsBuffer /*FramesInFlightData[currentFrame].drawBuffers._buffer.data*/, int currentFrame)
        //TODO GeneratehiZ(int currentFrame)
    }
      
    for(int j = 0; j < passes.size(); j++)
    {
        auto passInfo = passes[j];
        recordCommandBufferCulling(*passInfo.computeRenderStepContext, currentFrame, *passInfo.computeCullingInfo,indirectCommandsBuffer);
    }

    for(int j = 0; j < passes.size(); j++)
    {
        auto passInfo = passes[j];
        auto context = passInfo.drawRenderStepContext;
        assert(context->active);
        passInfo.pipelineGroup->bindToCommandBuffer(context->commandBuffer, currentFrame, 0); //Often the same -- cache?
        VkPipelineLayout layout =  passInfo.pipelineGroup->pipelineData.bindlessPipelineLayout; //Often the same -- cache?
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


struct pipelineBatch
{
    uint32_t pipelineIDX;
    Array<uint32_t> objectIndices; 
};

void vulkanRenderer::doMipChainCompute(ActiveRenderStepData commandBufferContext, MemoryArena::memoryArena* arena, VkImage dstImage,
    VkImageView srcView, std::span<VkImageView> pyramidviews, VkSampler sampler, uint32_t _currentFrame, int pyramidWidth, int pyramidHeight)
{
    
    assert(commandBufferContext.active);

    vkCmdBindPipeline(commandBufferContext.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, descriptorsetLayoutsDataMipChain.getPipeline(0));

    int i = 0;
   
 
    //For each texture, for each mip level, we want to work on, dispatch X/Y texture resolution and set a barrier (for the next phase)
    //Compute shader will read in texture and write back out to it
    VkImageMemoryBarrier2 barrier12 = imageBarrier(dstImage,
        0,
        0,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        0,
        0,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_IMAGE_ASPECT_COLOR_BIT,
        i,
        HIZDEPTH);

    
    pipelineBarrier(commandBufferContext.commandBuffer,0, 0, 0, 1, &barrier12);
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

        descriptorsetLayoutsDataMipChain.updateDescriptorSets(descriptorUpdates, _currentFrame, i);
        descriptorsetLayoutsDataMipChain.bindToCommandBuffer(commandBufferContext.commandBuffer, _currentFrame, i, VK_PIPELINE_BIND_POINT_COMPUTE);

        

        int outputWidth= pyramidWidth >> i;
        outputWidth = outputWidth < 1 ? 1 : outputWidth;
        int outputHeight = pyramidHeight >> i;
        outputHeight = outputHeight < 1 ? 1 : outputHeight;
        auto pushConstants = MemoryArena::Alloc<glm::vec2>(arena);
        *pushConstants = {outputWidth, outputHeight};
        vkCmdPushConstants(commandBufferContext.commandBuffer, descriptorsetLayoutsDataMipChain.pipelineData.bindlessPipelineLayout,VK_SHADER_STAGE_COMPUTE_BIT, 0,
                          sizeof(glm::vec2),pushConstants);

        //Dispatch for all the pixels?
        vkCmdDispatch(commandBufferContext.commandBuffer, outputWidth, outputHeight, 1);

//todo js
        VkImageMemoryBarrier2 barrier = imageBarrier(dstImage,
            0,
            0,
            VK_IMAGE_LAYOUT_GENERAL,
            0,
            0,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_ASPECT_COLOR_BIT,
            i,
            1);

        pipelineBarrier(commandBufferContext.commandBuffer,0, 0, 0, 1, &barrier);
    }

}

void vulkanRenderer::updateBindingsComputeCulling(ActiveRenderStepData commandBufferContext, 
    MemoryArena::memoryArena* arena, uint32_t _currentFrame)
{
    assert(commandBufferContext.active);
    VkDescriptorBufferInfo* frustumData =  MemoryArena::Alloc<VkDescriptorBufferInfo>(&perFrameArenas[_currentFrame], 1);
    *frustumData = getDescriptorBufferInfo(FramesInFlightData[_currentFrame].frustumsForCullBuffers);
    
    VkDescriptorBufferInfo* computeDrawBuffer = MemoryArena::Alloc<VkDescriptorBufferInfo>(&perFrameArenas[_currentFrame], 1); 
    *computeDrawBuffer = getDescriptorBufferInfo(FramesInFlightData[_currentFrame].drawBuffers);

    VkDescriptorBufferInfo* objectBufferInfo = MemoryArena::Alloc<VkDescriptorBufferInfo>(&perFrameArenas[_currentFrame], 1); 
    *objectBufferInfo = getDescriptorBufferInfo(FramesInFlightData[_currentFrame].uniformBuffers);
    
    std::span<descriptorUpdateData> descriptorUpdates = MemoryArena::AllocSpan<descriptorUpdateData>(arena, 3);

    descriptorUpdates[0] = {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, frustumData};  //frustum data
    descriptorUpdates[1] = {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, computeDrawBuffer}; //draws 
    descriptorUpdates[2] = {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, objectBufferInfo}; //objectData  //
    
    descriptorsetLayoutsDataCulling.updateDescriptorSets(descriptorUpdates, _currentFrame, 0);
    descriptorsetLayoutsDataCulling.bindToCommandBuffer(commandBufferContext.commandBuffer, _currentFrame, 0, VK_PIPELINE_BIND_POINT_COMPUTE);

    vkCmdBindPipeline(commandBufferContext.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, descriptorsetLayoutsDataCulling.getPipeline(0));
}



void vulkanRenderer::SubmitCommandBuffer(uint32_t commandbufferCt, ActiveRenderStepData* commandBufferContext, VkFence waitFence)
{

    assert(!commandBufferContext->active);
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    VkPipelineStageFlags _waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = (uint32_t)commandBufferContext->waitSemaphores.size();
    submitInfo.pWaitSemaphores = commandBufferContext->waitSemaphores.data();
    submitInfo.pWaitDstStageMask =_waitStages;
    submitInfo.commandBufferCount = commandbufferCt;
    submitInfo.pCommandBuffers = &commandBufferContext->commandBuffer;

    submitInfo.signalSemaphoreCount = commandBufferContext->signalSempahores.size();
    submitInfo.pSignalSemaphores = commandBufferContext->signalSempahores.data();
    
    //Submit pass 
    VK_CHECK(vkQueueSubmit(commandPoolmanager->Queues.graphicsQueue, 1,
        &submitInfo, waitFence));
    
    
    
}
void vulkanRenderer::recordUtilityPasses( VkCommandBuffer commandBuffer, int imageIndex)
{
    VkPipelineLayout layout = descriptorsetLayoutsData.pipelineData.bindlessPipelineLayout;
    VkRenderingAttachmentInfoKHR dynamicRenderingColorAttatchment = createRenderingAttatchmentStruct(rendererResources.swapchainImageViews[imageIndex], 0.0, false);
    VkRenderingAttachmentInfoKHR utilityDepthAttatchment =    createRenderingAttatchmentStruct( rendererResources.depthBufferInfo.view, 0.0, false);
    BeginRendering(
        &utilityDepthAttatchment,
        1, &dynamicRenderingColorAttatchment,
        rendererVulkanObjects.swapchain.extent,commandBuffer);
    
    //TODO JS: something other than hardcoded 2
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, descriptorsetLayoutsData.getPipeline(2));
    debugLines.push_back({{0,0,0},{20,40,20}, {0,0,1}});
    for (int i = 0; i <debugLines.size(); i++)
    {
    
        debugLinePConstants constants;
        //Light count, vert offset, texture index, and object data index
        constants.pos1 = glm::vec4(debugLines[i].start,1.0);
        constants.pos2 = glm::vec4(debugLines[i].end,1.0);
        constants.color = glm::vec4(debugLines[i].color,1.0);
        constants.m = glm::mat4(1.0);
    
        vkCmdPushConstants(commandBuffer, layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                          sizeof(debugLinePConstants), &constants);
    
        vkCmdDraw(commandBuffer, 2, 1, 0, 0);
    }


    //temp home for imgui
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);

    
    vkCmdEndRendering(commandBuffer);


}

// for(int i =0; i < meshct; i++)
// {
//     auto scalefactor = glm::max(glm::max(abs(scene->objects.scales[i].x),abs(scene->objects.scales[i].y)), abs(scene->objects.scales[i].z));
//
//     positionRadius bounds = AssetDataAndMemory->meshBoundingSphereRad[scene->objects.meshIndices[i]];
//     auto lookup = scene->transforms._transform_lookup[i];
//     glm::mat4 model = scene->transforms.worldMatrices[lookup.depth][lookup.index];
//     glm::vec3 corner1 = AssetDataAndMemory->backing_meshes[scene->objects.meshIndices[i]].boundsCorners[0];
//     glm::vec3 corner2 = AssetDataAndMemory->backing_meshes[scene->objects.meshIndices[i]].boundsCorners[1];
//
// }

#pragma endregion


bool vulkanRenderer::hasStencilComponent(VkFormat format)
{
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

#pragma region depth

//zeux code
uint32_t previousPow2(uint32_t v)
{
    uint32_t r = 1;

    while (r * 2 < v)
        r *= 2;

    return r;
}


DepthBufferInfo static_createDepthResources(rendererObjects initializedrenderer,  MemoryArena::memoryArena* allocationArena, RendererDeletionQueue* deletionQueue, CommandPoolManager* commandPoolmanager )
{
    auto depthFormat = Capabilities::findDepthFormat(initializedrenderer.vkbPhysicalDevice.physical_device);
    VkImage depthImage;
                                            //The two null context will get overwritten by the create  calls below
    DepthBufferInfo bufferInfo = {depthFormat, VK_NULL_HANDLE, VK_NULL_HANDLE, VmaAllocation {}};


    BufferCreationContext context = {
        .device =  initializedrenderer.vkbdevice.device,
        .allocator =  initializedrenderer.vmaAllocator,
        .rendererdeletionqueue = deletionQueue,
        .commandPoolmanager = commandPoolmanager
    };
    

    TextureUtilities::createImage(context, initializedrenderer.swapchain.extent.width, initializedrenderer.swapchain.extent.height, bufferInfo.format,
                                  VK_IMAGE_TILING_OPTIMAL,
                                  VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                  (bufferInfo.image),
                                 (bufferInfo.vmaAllocation), 1, 1, false, "DEPTH TEXTURE");

    bufferInfo.view = TextureUtilities::createImageViewCustomMip(context,   (bufferInfo.image), bufferInfo.format, VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_VIEW_TYPE_2D, 1, 0,1, 0);

return bufferInfo;
}

DepthPyramidInfo static_createDepthPyramidResources(rendererObjects initializedrenderer,  MemoryArena::memoryArena* allocationArena, RendererDeletionQueue* deletionQueue, CommandPoolManager* commandPoolmanager )
{
    auto depthFormat = Capabilities::findSupportedFormat(initializedrenderer.vkbPhysicalDevice.physical_device,
                               {VK_FORMAT_R16_SFLOAT},
                               VK_IMAGE_TILING_OPTIMAL,
                               VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT );
    //The two null context will get overwritten by the create  calls below
    std::span<VkImageView> viewsForMips = MemoryArena::AllocSpan<VkImageView>(allocationArena, HIZDEPTH);
    int depthWidth = previousPow2(initializedrenderer.swapchain.extent.width);
    int depthHeight = previousPow2(initializedrenderer.swapchain.extent.height);
    DepthPyramidInfo bufferInfo = {depthFormat, VK_NULL_HANDLE, viewsForMips,  VmaAllocation {}, {depthWidth, depthHeight}};


    BufferCreationContext context = {
        .device =  initializedrenderer.vkbdevice.device,
        .allocator =  initializedrenderer.vmaAllocator,
        .rendererdeletionqueue = deletionQueue,
        .commandPoolmanager = commandPoolmanager
    };


    TextureUtilities::createImage(context, depthWidth,depthHeight, bufferInfo.format,
                                  VK_IMAGE_TILING_OPTIMAL,
                                  VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                  (bufferInfo.image),
                                 (bufferInfo.vmaAllocation), HIZDEPTH, 1, false, "DEPTH PYRAMID TEXTURE");
    for(int i = 0; i < bufferInfo.viewsForMips.size(); i++)
    {
        bufferInfo.viewsForMips[i] =
            TextureUtilities::createImageViewCustomMip(context,   (bufferInfo.image), bufferInfo.format, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_VIEW_TYPE_2D, 1, 0,1, i);
    }

    auto tempBufferAndPool = commandPoolmanager->beginSingleTimeCommands(false);
    VkImageMemoryBarrier2 barrier11 = imageBarrier(bufferInfo.image,
     0,
     0,
     VK_IMAGE_LAYOUT_UNDEFINED,
     0,
     0,
     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
     VK_IMAGE_ASPECT_COLOR_BIT,
     0,
     HIZDEPTH);
    pipelineBarrier(tempBufferAndPool.buffer,0, 0, 0, 1, &barrier11);
    commandPoolmanager->endSingleTimeCommands(tempBufferAndPool);

    return bufferInfo;
}

#pragma endregion



void vulkanRenderer::createGraphicsPipeline(std::span<VkPipelineShaderStageCreateInfo> shaderStages, PipelineGroup* descriptorsetdata,  PipelineGroup::graphicsPipelineSettings settings, bool compute, size_t pconstantSize)
{ 
    if (!compute)
    descriptorsetdata->createGraphicsPipeline(shaderStages, settings, pconstantSize);
    else
        descriptorsetdata->createComputePipeline(shaderStages[0], pconstantSize);
 

}




#pragma region perFrameUpdate


int i =0;
void vulkanRenderer::Update(Scene* scene)
{


    ImGui::Begin("RENDERER Buffer preview window");
    ImGui::Text("TODO");
    int shadowIndex =1;
    VkDescriptorSet img = GetOrRegisterImguiTextureHandle(rendererResources.shadowSamplers[shadowIndex],rendererResources.shadowMapSamplingImageViews[shadowIndex][3]);
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

        if (!firstTime[currentFrame])
        {
            vkWaitForFences(rendererVulkanObjects.vkbdevice.device, 1, &FramesInFlightData[currentFrame].inFlightFence, VK_TRUE, UINT64_MAX);
            FramesInFlightData[currentFrame].deletionQueue->FreeQueue();
            MemoryArena::free(&perFrameArenas[currentFrame]);
       
            vkResetFences(rendererVulkanObjects.vkbdevice.device, 1, &FramesInFlightData[currentFrame].inFlightFence);
      
        }
        if (firstTime[currentFrame])
        {
            firstTime[currentFrame] = false;
        }
        AssetDataAndMemory->Update();

        //This stuff belongs here
     
        drawFrame(scene);
        debugLines.clear();

        auto t2 = SDL_GetTicks();
    
    // vkDeviceWaitIdle(rendererVulkanObjects.vkbdevice.device);
}



semaphoreData getSemaphoreDataFromSemaphores(std::span<VkSemaphore> semaphores, MemoryArena::memoryArena* allocator)
{
    auto flags = MemoryArena::AllocSpan<VkPipelineStageFlags>(allocator, semaphores.size());
    for (int i = 0; i <flags.size(); i++)
    {
        flags[i] = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    }
    return {semaphores, flags};
    
}
void transitionImageForRendering(RendererContext context, ActiveRenderStepData* RenderStepContext, VkImage image, VkImageLayout layoutIn, VkImageLayout layoutOut, VkPipelineStageFlags* waitStages, int mipCount, bool depth)
{
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = 0; // Optional
    beginInfo.pInheritanceInfo = nullptr; // Optional
    //Transition swapchain for rendering
    
    TextureUtilities::transitionImageLayout(objectCreationContextFromRendererContext(context),image,(VkFormat)0,layoutIn,layoutOut,RenderStepContext->commandBuffer,mipCount, false, depth);

  
    VkSubmitInfo swapChainInSubmitInfo{};
    VkPipelineStageFlags _waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    swapChainInSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    swapChainInSubmitInfo.commandBufferCount = 1;
    swapChainInSubmitInfo.pWaitDstStageMask = _waitStages;
    swapChainInSubmitInfo.pCommandBuffers = &RenderStepContext->commandBuffer;
    swapChainInSubmitInfo.waitSemaphoreCount = (uint32_t)RenderStepContext->waitSemaphores.size();
    swapChainInSubmitInfo.pWaitSemaphores =RenderStepContext->waitSemaphores.data();
    swapChainInSubmitInfo.signalSemaphoreCount = RenderStepContext->signalSempahores.size();
    swapChainInSubmitInfo.pSignalSemaphores = RenderStepContext->signalSempahores.data();

    ///////////////////////// Transition swapChain  />
    
}
VkPipelineStageFlags swapchainWaitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
VkPipelineStageFlags shadowWaitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

bool getFlattenedShadowData(int index, std::span<std::span<PerShadowData>> inputShadowdata, PerShadowData* result)
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


RenderPassConfig CreateRenderPassConfig_new(MemoryArena::memoryArena* allocator, Scene* scene,  AssetManager* rendererData,
                                    renderPassAttatchmentInfo renderAttatchmentInfo,
                                    shaderLookup shaderGroup,
                                     PipelineGroup* computePipelineData, 
                                     ActiveRenderStepData* shadowRenderStepContext,
                                     ActiveRenderStepData* computeRenderStepContext, 
                                     pointerSize pushConstantReference,
                                      VkBuffer indexBuffer, viewProj cameraViewProj, uint32_t drawOffset, uint32_t passOffset, uint32_t objectCount, depthBiasSettng depthBiasConfig )
{
    cullPConstants* cullconstants = MemoryArena::Alloc<cullPConstants>(allocator);
    *cullconstants = {.view = cameraViewProj.view, .firstDraw = drawOffset, .frustumIndex = (passOffset) * 6, .objectCount = objectCount};
    ComputeCullListInfo* cullingInfo = MemoryArena::Alloc<ComputeCullListInfo>(allocator);
    *cullingInfo =  {.firstDrawIndirectIndex = drawOffset, .drawCount = objectCount, .viewMatrix = cameraViewProj.view, .projMatrix = cameraViewProj.proj, 
        .layout = computePipelineData->pipelineData.bindlessPipelineLayout, // just get pipeline layout globally?
       .pushConstantInfo =  {.ptr = cullconstants, .size =  sizeof(cullPConstants) }};

    Array<pipelineBatch> batchedDrawBuckets = MemoryArena::AllocSpan<pipelineBatch>(allocator, shaderGroup.shaderIndices.size());
    ////Flatten buckets into simpleMeshPassInfos sorted by pipeline
    ///    //Opaque passes are bucketed by pipeline 
    //initialize and fill pipeline buckets
    for (uint32_t i = 0; i < shaderGroup.shaderIndices.size(); i++)
    {
        if (shaderGroup.shaderIndices[i] >= batchedDrawBuckets.size())
        {
            batchedDrawBuckets.push_back( {i, Array(MemoryArena::AllocSpan<uint32_t>(allocator, MAX_DRAWS_PER_PIPELINE))});
        }
    }
    for (uint32_t i = 0; i < objectCount; i++)
    {
        int shaderGroupIndex = rendererData->materials[scene->objects.materials[i]].shaderGroupIndex;
        int pipelineIndex = shaderGroup.shaderIndices[shaderGroupIndex];
        batchedDrawBuckets[pipelineIndex].objectIndices.push_back(i);
    }
    
    Array meshPasses = MemoryArena::AllocSpan<simpleMeshPassInfo>(allocator, MAX_PIPELINES);
    uint32_t drawOffsetAtTheStartOfOpaque = drawOffset;
    int subspanOffset = 0;
    for (size_t i = 0; i < batchedDrawBuckets.size(); i++)
    {
        auto drawBatch =  batchedDrawBuckets[i];
        if (drawBatch.objectIndices.size() == 0)
        {
            continue;
        }
        Array<uint32_t> indices = drawBatch.objectIndices;
        auto firstIndex = drawOffsetAtTheStartOfOpaque + subspanOffset;

        int shaderID = shaderGroup.shaderIndices[drawBatch.pipelineIDX];
        
        meshPasses.push_back({.firstIndex = firstIndex, .ct = (uint32_t)indices.ct, .pipeline = shaderGroup.pipelineGroup->getPipeline(shaderID),
            .sortedObjectIDs =  drawBatch.objectIndices.getSpan()});

        subspanOffset += indices.ct;
    }


  return {
             .drawRenderStepContext =  shadowRenderStepContext,
             .computeRenderStepContext = computeRenderStepContext,
             .pipelineGroup = shaderGroup.pipelineGroup ,
             .indexBuffer =indexBuffer,
             .indexBufferType =  VK_INDEX_TYPE_UINT32,
             .meshPasses = meshPasses.getSpan(),
             .computeCullingInfo = cullingInfo,
             .depthAttatchment = renderAttatchmentInfo.depthDraw,
             .colorattatchment = renderAttatchmentInfo.colorDraw,
             .renderingAttatchmentExtent = renderAttatchmentInfo.extents,
      .matrices =  cameraViewProj,
             .pushConstants = pushConstantReference.ptr,
             .pushConstantsSize = pushConstantReference.size,
             //TODO JS: dynamically set bias per shadow caster, especially for cascades
             .depthBiasSetting = depthBiasConfig};
}


void    AddOpaquePasses(RenderPassList* targetPassList, shaderLookup shaderGroup, Scene* scene, AssetManager* rendererData,
                                         MemoryArena::memoryArena* allocator,
                                     std::span<std::span<PerShadowData>> inputShadowdata,
                                     PipelineGroup* computePipelineData,
                                     ActiveRenderStepData* opaqueRenderStepContext,
                                     ActiveRenderStepData* computeRenderStepContext,
                                     VkRenderingAttachmentInfoKHR* opaqueTarget, VkRenderingAttachmentInfoKHR* depthTarget, VkExtent2D targetExtent, VkBuffer indexBuffer)
    
{
    uint32_t objectsPerDraw = scene->objectsCount();
    viewProj viewProjMatrices = viewProjFromCamera(scene->sceneCamera);

    
    //Culling debug stuff
    
    PerShadowData* data = MemoryArena::Alloc<PerShadowData>(allocator);
    int shadowDrawCt = 0;
    for(int i =0; i <inputShadowdata.size(); i++)
    {
        shadowDrawCt += inputShadowdata[i].size();
    }
    assert(targetPassList->drawCount >= shadowDrawCt); //TODO JS-frustum-binding

    
    if ( debug_cull_override && getFlattenedShadowData(debug_cull_override_index,inputShadowdata, data))
    {
        viewProjMatrices = { data->view,  data->proj};
        glm::vec4 frustumCornersWorldSpace[8] = {};
        populateFrustumCornersForSpace(frustumCornersWorldSpace,   glm::inverse(viewProjMatrices.proj * viewProjMatrices.view));
        debugDrawFrustum(frustumCornersWorldSpace);
    }

   
    targetPassList->passes.push_back(  CreateRenderPassConfig_new(allocator, scene, rendererData,
        {.colorDraw = opaqueTarget, .depthDraw = depthTarget, .extents = targetExtent},
        shaderGroup, computePipelineData, opaqueRenderStepContext, computeRenderStepContext,
        {.ptr = nullptr, .size = 0}, indexBuffer,viewProjMatrices,targetPassList->drawCount,targetPassList->passes.size(), objectsPerDraw,{false, 0, 0}
        ));
    targetPassList->drawCount += objectsPerDraw;

}

void AddShadowPasses(RenderPassList* targetPassList, Scene* scene, AssetManager* rendererData,
                                     cameraData* camera, MemoryArena::memoryArena* allocator,
                                     std::span<std::span<PerShadowData>> inputShadowdata,
                                   shaderLookup shaderLookup,
                                     PipelineGroup* computePipelineData,
                                     ActiveRenderStepData* shadowRenderStepContext,
                                     ActiveRenderStepData* computeRenderStepContext,
                                      VkBuffer indexBuffer,   std::span<VkImageView> shadowMapRenderingViews)
{
    uint32_t objectsPerDraw =scene->objectsCount();
    
    for(int i = 0; i < glm::min(scene->lightCount, MAX_SHADOWCASTERS); i ++)
    {
        lightType type = (lightType)scene->lightTypes[i];
        int lightSubpasses = shadowCountFromLightType(type);
        for (int j = 0; j < lightSubpasses; j++)
        {
            auto view = inputShadowdata[i][j].view;
            auto proj =  inputShadowdata[i][j].proj;
            //todo js: wanna rethink this, since shadow rendering is like effectively dupe with depth prepass rendering 
            shadowPushConstants* shadowPC = MemoryArena::Alloc<shadowPushConstants>(allocator);
            shadowPC ->matrix =  proj * view;
            
            VkRenderingAttachmentInfoKHR* depthDrawAttatchment = MemoryArena::Alloc<VkRenderingAttachmentInfoKHR>(allocator);
            *depthDrawAttatchment =  createRenderingAttatchmentStruct(shadowMapRenderingViews[targetPassList->passes.size()], 1.0, true);
            
            targetPassList->passes.push_back(
            CreateRenderPassConfig_new(
                allocator, scene, rendererData,
                {.colorDraw = VK_NULL_HANDLE, .depthDraw = depthDrawAttatchment, .extents = {SHADOW_MAP_SIZE, SHADOW_MAP_SIZE}},
                shaderLookup,
                computePipelineData,
                shadowRenderStepContext,
                computeRenderStepContext,
                {.ptr = shadowPC, .size =sizeof(shadowPushConstants)},
                indexBuffer,
                {.view = view, .proj =proj },
                targetPassList->drawCount,
                targetPassList->passes.size(),
                objectsPerDraw,{.use = true, .depthBias = 6.0, .slopeBias = 3.0})
                );
            
            targetPassList->drawCount += objectsPerDraw;
        }
    }
   
}

int UpdateDrawCommanddataDrawIndirectCommands(std::span<drawCommandData> targetDrawCommandSpan, std::span<uint32_t> objectIDtoSortedObjectID, std::span<uint32_t> objectIDtoMeshID, std::span<uint32_t>meshIDtoFirstIndex, std::span<uint32_t> meshIDtoIndexCount)
{
    for (size_t j = 0; j < targetDrawCommandSpan.size(); j++)
    {
        auto objectDrawIndex = objectIDtoSortedObjectID[j];
        auto meshID = objectIDtoMeshID[objectDrawIndex];
        targetDrawCommandSpan[j] = {
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

//This function maps all of our candidate draws to draw indirect buffers
//We end up with the gpu mapped DrawCommands populated with a unique list of drawcommanddata for each object for each draw
//I think I did it this way so I can easily set index count to zero later on to skip a draw
//Would be better to only have one list of objects to draw?
void uploadIndirectCommandBufferForPasses(Scene* scene, AssetManager* rendererData, MemoryArena::memoryArena* allocator, std::span<drawCommandData> drawCommands, std::span<RenderPassConfig> passes)
{
    for(int i =0; i < drawCommands.size(); i++)
    {
        drawCommands[i].objectIndex = -1;
    }
   
    uint32_t objectsPerDraw = scene->objectsCount();
    
    std::span<uint32_t> objectIDtoMeshID = MemoryArena::AllocSpan<uint32_t>(allocator, objectsPerDraw);
    std::span<uint32_t> objectIDtoSortedObjectID = MemoryArena::AllocSpan<uint32_t>(allocator, objectsPerDraw);
    std::span<uint32_t> meshIDtoFirstIndex = MemoryArena::AllocSpan<uint32_t>(allocator, rendererData->backing_meshes.size());
    std::span<uint32_t> meshIDtoIndexCount = MemoryArena::AllocSpan<uint32_t>(allocator, rendererData->backing_meshes.size());
    uint32_t indexCtoffset = 0;
    for(size_t i = 0; i < objectsPerDraw; i++)
    {
        objectIDtoMeshID[i] = scene->objects.meshIndices[i];
        objectIDtoSortedObjectID[i] = i; //
    }
    for(size_t i = 0; i < rendererData->backing_meshes.size(); i++)
    {
        meshIDtoFirstIndex[i] = indexCtoffset;
        meshIDtoIndexCount[i] =rendererData->backing_meshes[i].indices.size();
        indexCtoffset += rendererData->backing_meshes[i].indices.size();
    }

    
    //indirect draw commands for shadows/depth only draws
    for(size_t i =0; i < passes.size(); i++)
    {
        auto _pass =passes[i];
        for (int j = 0; j < _pass.meshPasses.size(); j++)
        {
            auto pass = _pass.meshPasses[j];
            UpdateDrawCommanddataDrawIndirectCommands(drawCommands.subspan(pass.firstIndex, pass.ct), pass.sortedObjectIDs, objectIDtoMeshID,meshIDtoFirstIndex, meshIDtoIndexCount);
        }
    }
}

void AddBufferTrasnfer(VkBuffer sourceBuffer, VkBuffer targetBuffer, size_t copySize, VkCommandBuffer cmdBuffer)
{
         VkBufferMemoryBarrier bufMemBarrier = { VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };
        bufMemBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
        bufMemBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        bufMemBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        bufMemBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        bufMemBarrier.buffer = sourceBuffer;
        bufMemBarrier.offset = 0;
        bufMemBarrier.size = VK_WHOLE_SIZE;
        vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            0, 0, nullptr, 1, &bufMemBarrier, 0, nullptr);
        
            
            //Copy vertex data over
            BufferUtilities::copyBufferWithCommandBuffer(cmdBuffer, sourceBuffer,
                targetBuffer, copySize);
            VkBufferMemoryBarrier bufMemBarrier2 = { VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };
            bufMemBarrier2.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            bufMemBarrier2.dstAccessMask = VK_ACCESS_UNIFORM_READ_BIT; // We created a uniform buffer
            bufMemBarrier2.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            bufMemBarrier2.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            bufMemBarrier2.buffer = targetBuffer;
            bufMemBarrier2.offset = 0;
            bufMemBarrier2.size = VK_WHOLE_SIZE;
            vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
                0, 0, nullptr, 1, &bufMemBarrier2, 0, nullptr);

}

ActiveRenderStepData createAndBeginRenderStep(VkDevice device, const char* debugName, const char* cbufferDebugName,  RendererDeletionQueue* deletionQueue, VkCommandPool pool,     std::span<VkSemaphore> waitSemaphores,
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

uint32_t internal_debug_cull_override_index = 0;
void vulkanRenderer::drawFrame(Scene* scene)
{
    if (debug_cull_override_index != internal_debug_cull_override_index)
    {
        debug_cull_override_index %= scene->getShadowDataIndex(glm::min(scene->lightCount, MAX_SHADOWCASTERS));
        internal_debug_cull_override_index = debug_cull_override_index;
        printf("new cull index: %d! \n",  debug_cull_override_index);
    }
    //Update per frame data

    //Prepare for pass
    uint32_t swapChainImageIndex;
    per_frame_data* thisFrameData = &FramesInFlightData[currentFrame];
    acquireImageSemaphore acquireImageSemaphore = thisFrameData->semaphores;
    vkAcquireNextImageKHR(rendererVulkanObjects.vkbdevice.device, rendererVulkanObjects.swapchain,
        UINT64_MAX,acquireImageSemaphore.semaphore, VK_NULL_HANDLE,&swapChainImageIndex);
    
    //Sync data updated from the engine
    updateShadowData(&perFrameArenas[currentFrame], perLightShadowData, scene, scene->sceneCamera);
    updatePerFrameBuffers(currentFrame, scene->transforms.worldMatrices,scene); 

    updateOpaqueDescriptorSets(&descriptorsetLayoutsData);
    updateShadowDescriptorSets(&descriptorsetLayoutsDataShadow, 0);
    

    //Kinda a convenience thing, todo more later, wrote in a hurry
    struct renderStepContextObject
    {
        VkDevice device;
        RendererDeletionQueue* deletionQueue;
         Array<ActiveRenderStepData> renderstepDatas;
        Array<VkSemaphore> semaphorePool;
        std::span<VkSemaphore> previousStepSignalSemaphores;

        //Set up a command buffer, initialize it, return an object wrapping all the relevant stuff
        //Default to chaining semaphores with the last/next one
        ActiveRenderStepData* createRenderStepAndInitialize(const char* debugName, const char* cbufferDebugName,VkCommandPool pool,
            bool overrideWaitSemaphores, std::span<VkSemaphore> overridenWaitSemaphores,
             bool overrideSignalSemaphores, std::span<VkSemaphore> overridenSignalSemaphores)
        {

            assert(!(!overrideWaitSemaphores && renderstepDatas.ct == 0));  //We can only override wait semaphores if we have prior steps. 
            
            auto signalSemaphores = overrideSignalSemaphores ? overridenSignalSemaphores : semaphorePool.pushUninitializedSubspan(1);
            auto waitSemaphores = overrideWaitSemaphores ? overridenWaitSemaphores : previousStepSignalSemaphores;
            previousStepSignalSemaphores = signalSemaphores;
            VkCommandBufferAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            allocInfo.commandPool = pool;
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

            renderstepDatas.push_back({.active = true, .indexBuffer = VK_NULL_HANDLE, .boundPipeline = VK_NULL_HANDLE,
                .commandBuffer = commandBuffer, .waitSemaphores = waitSemaphores, .signalSempahores = signalSemaphores });

            return &renderstepDatas.back();
        }

        void finishAndSubmitCommandBuffers()
        {
            ////Submit our render steps
            for (int j = 0; j < renderstepDatas.size(); j++)
            {
                ActiveRenderStepData* ctx = &renderstepDatas[j];
                endCommandBufferRecording(ctx);
                SubmitCommandBuffer(1, ctx, ctx->fence != nullptr? *ctx->fence : VK_NULL_HANDLE);
            }
        }
        
    };

    renderStepContextObject renderstepDatas =
        {
        .device =   rendererVulkanObjects.vkbdevice.device,
        .deletionQueue = thisFrameData->deletionQueue.get(),
        .renderstepDatas =  MemoryArena::AllocSpan<ActiveRenderStepData>(&perFrameArenas[currentFrame], 20),
        .semaphorePool = MemoryArena::AllocSpan<VkSemaphore>(&perFrameArenas[currentFrame], 100)
        };

    //I'm currently using multiple command buffers to handle most of my concurrency
    //each one of these step datas gets a signal and wait semaphore list, and then their command buffers are all submitted in order in the loop near the end of this fn
    //This needs revisted, but not right away
    //Create command buffers for this frame and allocate their signal semaphores/set up their dependencies. Written here in linear order

    ///Swapchain transition in
    auto preRenderTransitionsrenderStepContext = renderstepDatas.createRenderStepAndInitialize(
        "swapTransitionInSignalSemaphores", "swapTransitionInCommandBuffer",
        commandPoolmanager->commandPool,
        true, std::span<VkSemaphore>(&acquireImageSemaphore.semaphore, 1),
        false, {});

    //Compute frustum culling
    auto computeRenderStepContext = renderstepDatas.createRenderStepAndInitialize(
        "frustumSignalSemaphores", "frustumCommandBuffer",
        commandPoolmanager->commandPool, false, {},
        false, {});

    //Depth Prepass
    auto cullingDepthPrepassRenderStepContext = renderstepDatas.createRenderStepAndInitialize(
        "prepassSignalSemaphores", "prepassCommandBuffer",
        commandPoolmanager->commandPool, false,
        {}, false, {});

    //Mip Chain Generation
    //This will be pretty iddle, right? Would rather be going ahead with other rendering 
    //Might be the first place I need more fine grained synchronization
    auto mipChainRenderStepContext = renderstepDatas.createRenderStepAndInitialize(
        "MipChainSignalSemaphores", "MipChainCommandBuffer",
        commandPoolmanager->commandPool, false,
        {}, false, {});

    //Shadow map Drawing 
    auto shadowRenderStepContext = renderstepDatas.createRenderStepAndInitialize(
        "shadowSignalSemaphores", "shadowCommandBuffer",
        commandPoolmanager->commandPool, false,
        {}, false, {});
    auto beforeOpaqueTransitionOutRenderStepContext = renderstepDatas.createRenderStepAndInitialize(
        "shadowTransitionOutSignalSemaphores", "shadowTransitionOutCommandBuffer",
        commandPoolmanager->commandPool, false,
        {}, false, {});
    //Transition Shadowmaps

    //Opaque rendering 
    auto opaqueRenderStepContext = renderstepDatas.createRenderStepAndInitialize(
        "opaqueSignalSemaphores", "opaqueCommandBuffer",
        commandPoolmanager->commandPool, false,
        {}, false, {});
    opaqueRenderStepContext->fence = &FramesInFlightData[currentFrame].inFlightFence;

    auto postRenderTransitionOutStepContext = renderstepDatas.createRenderStepAndInitialize(
        "TR_RenderingSignalSemaphores", "TR_RenderingCommandBuffer",
        commandPoolmanager->commandPool, false,
        {}, false, {});

    //TODO JS: it's possible I'm leaking tons of commandbuffers into vram here? May need to add them to flush per frame pools or something?
    //TODO JS: Can add cleanup to the renderstepDatas for loop at the end?

    //Pre-rendering setup
    //  // //
    transitionImageForRendering(
    getFullRendererContext(),
    preRenderTransitionsrenderStepContext,
    rendererResources.swapchainImages[swapChainImageIndex],
    VK_IMAGE_LAYOUT_UNDEFINED,
    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, swapchainWaitStages, 1, false);
    
    transitionImageForRendering(
    getFullRendererContext(),
    preRenderTransitionsrenderStepContext,
    rendererResources.shadowImages[currentFrame],
    VK_IMAGE_LAYOUT_UNDEFINED,
    VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, shadowWaitStages, 1, true);
    
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
    doMipChainCompute(*mipChainRenderStepContext, &perFrameArenas[currentFrame],  rendererResources.depthPyramidInfo.image,
                      rendererResources.depthBufferInfo.view, rendererResources.depthPyramidInfo.viewsForMips, rendererResources.depthMipSampler, currentFrame, rendererResources.depthPyramidInfo.depthSize.x,
                       rendererResources.depthPyramidInfo.depthSize.y);     
    //Create the renderpass
     auto* mainRenderTargetAttatchment = MemoryArena::Alloc<VkRenderingAttachmentInfoKHR>(&perFrameArenas[currentFrame]);
     *mainRenderTargetAttatchment =  createRenderingAttatchmentStruct(rendererResources.swapchainImageViews[swapChainImageIndex], 0.0, true);

    //set up all our  draws
    RenderPassList* renderPasses = MemoryArena::Alloc<RenderPassList>(&perFrameArenas[currentFrame]);
    *renderPasses = {.drawCount = 0,  .passes =  MemoryArena::AllocSpan<RenderPassConfig>(&perFrameArenas[currentFrame], MAX_RENDER_PASSES)};

     AddShadowPasses(renderPasses, scene, AssetDataAndMemory, &scene->sceneCamera,
                     &perFrameArenas[currentFrame],
                     perLightShadowData, shaderGroups.shadowShaders, &descriptorsetLayoutsDataCulling
                     ,  shadowRenderStepContext,  computeRenderStepContext,thisFrameData->deviceIndices.data, rendererResources.shadowMapRenderingImageViews[currentFrame]);

    VkRenderingAttachmentInfoKHR* depthDrawAttatchment = MemoryArena::Alloc<VkRenderingAttachmentInfoKHR>(&perFrameArenas[currentFrame]);
    *depthDrawAttatchment = createRenderingAttatchmentStruct( rendererResources.depthBufferInfo.view, 1.0, true);
    
    AddOpaquePasses(renderPasses,shaderGroups.opaqueShaders, scene,  AssetDataAndMemory, &perFrameArenas[currentFrame],
                    perLightShadowData,
                    &descriptorsetLayoutsDataCulling, opaqueRenderStepContext
                    ,  computeRenderStepContext,   mainRenderTargetAttatchment,depthDrawAttatchment, rendererVulkanObjects.swapchain.extent,
                 thisFrameData->deviceIndices.data);


    //Indirect command buffer
    uploadIndirectCommandBufferForPasses(scene, AssetDataAndMemory, &perFrameArenas[currentFrame], thisFrameData->drawBuffers.getMappedSpan(), renderPasses->passes.getSpan());
    updateBindingsComputeCulling(*computeRenderStepContext,  &perFrameArenas[currentFrame], currentFrame);
    
 
    //Prototype depth passes code
    //Going to move creation of these into the add passes fns, and have the logic to look up the appropriate sahder passed thru there
    //Need to think about how 
    auto passes = renderPasses->passes.getSpan();
    for(size_t i = 0; i < passes.size(); i++)
    {
        auto oldPass = passes[i];
        auto newPass =  passes[i];
        newPass.colorattatchment = VK_NULL_HANDLE;
        std::span<simpleMeshPassInfo> oldPasses = passes[i].meshPasses;
        std::span<simpleMeshPassInfo> newPasses = MemoryArena::copySpan<simpleMeshPassInfo>(&perFrameArenas[currentFrame], oldPasses);
        for(int j =0; j < newPasses.size(); j++)
        {
            newPasses[j].pipeline = shaderGroups.shadowShaders.pipelineGroup->getPipeline(1); //TODO JS: Avoid hardcoded index 1 for cull shader
        }
        newPass.meshPasses = newPasses;
        newPass.drawRenderStepContext = cullingDepthPrepassRenderStepContext;
        newPass.depthAttatchment = MemoryArena::AllocCopy<VkRenderingAttachmentInfoKHR>(&perFrameArenas[currentFrame], *passes[i].depthAttatchment);
        if (newPass.pushConstantsSize == 0) //kludge -- this is a opaque draw
        {
            newPass.depthBiasSetting = {true, 0, 0};
            for(int j =0; j < newPasses.size(); j++)
            {
                newPasses[j].pipeline = shaderGroups.opaqueShaders.pipelineGroup->getPipeline(shaderGroups.opaqueShaders.pipelineGroup->getPipelineCt() -1); //TODO JS: Avoid hardcoded last index for non-shadow cull shader
            }
        }
        renderPasses->passes.push_back(newPass);
        passes[i].depthAttatchment->loadOp = VK_ATTACHMENT_LOAD_OP_LOAD; ////TODO JS: as I work on depth prepass -- later set this when the pass is created
    }

    
    //Submti the actual Draws
    recordPrimaryRenderPasses(renderPasses->passes.getSpan(),thisFrameData->drawBuffers.buffer.data, currentFrame);
    recordUtilityPasses(opaqueRenderStepContext->commandBuffer, swapChainImageIndex);
    //

    //After render steps
    //
    //
    
    transitionImageForRendering(
      getFullRendererContext(),
      beforeOpaqueTransitionOutRenderStepContext,
      rendererResources.shadowImages[currentFrame],
      VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, shadowWaitStages, 1, true);
    transitionImageForRendering(
        getFullRendererContext(),
        beforeOpaqueTransitionOutRenderStepContext,
        rendererResources.swapchainImages[swapChainImageIndex],
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, swapchainWaitStages, 1, false);

    ////Submit our render steps
    renderstepDatas.finishAndSubmitCommandBuffers();



    //Render
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = postRenderTransitionOutStepContext->signalSempahores.size();
    presentInfo.pWaitSemaphores = postRenderTransitionOutStepContext->signalSempahores.data();
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = {&rendererVulkanObjects.swapchain.swapchain};
    presentInfo.pImageIndices = &swapChainImageIndex;
    presentInfo.pResults = nullptr; // Optional

    //Present frame
    vkQueuePresentKHR(commandPoolmanager->Queues.presentQueue, &presentInfo);
    

    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void vulkanRenderer::cleanup()
{

    for(int i =0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        vkWaitForFences(rendererVulkanObjects.vkbdevice.device, 1, &FramesInFlightData[i].inFlightFence, VK_TRUE, UINT64_MAX);
    }
    //
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    //

    deletionQueue->FreeQueue();


    // vkDestroyDescriptorPool(device, descriptorPool, nullptr);
    descriptorsetLayoutsData.cleanup();
    descriptorsetLayoutsDataShadow.cleanup();
    descriptorsetLayoutsDataCulling.cleanup();
    AssetDataAndMemory->Cleanup(); //noop currently
 



     // vkb::destroy_surface(vkb_surface);
    destroy_swapchain(rendererVulkanObjects.swapchain);
   
    vkDestroySurfaceKHR(rendererVulkanObjects.vkbInstance.instance, rendererVulkanObjects.surface, nullptr);

   vkDeviceWaitIdle(rendererVulkanObjects.vkbdevice.device);
   vmaDestroyAllocator(rendererVulkanObjects.vmaAllocator);
   destroy_device(rendererVulkanObjects.vkbdevice);
   destroy_instance(rendererVulkanObjects.vkbInstance);
   SDL_DestroyWindow(_window);

 
   
}

#pragma endregion

void printGraph(localTransform g, int depth)
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
            printGraph(*g.children[i], depth);
        }
    }
}




//could be static utility, move
//Throw a bunch of VUID-vkCmdDrawIndexed-viewType-07752 errors due to image format, but renders fine
VkDescriptorSet vulkanRenderer::GetOrRegisterImguiTextureHandle(VkSampler sampler, VkImageView imageView)
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
void vulkanRenderer::initializeDearIMGUI()
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
    init_info.QueueFamily = commandPoolmanager->Queues.graphicsQueueFamily;
    init_info.Queue = commandPoolmanager->Queues.graphicsQueue;
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
    init_info.PipelineRenderingCreateInfo.depthAttachmentFormat = rendererResources.depthBufferInfo.format;
    //init_info.Allocator = nullptr; //
    //init_info.CheckVkResultFn = check_vk_result;
    ImGui_ImplVulkan_Init(&init_info);
    ImGui_ImplVulkan_CreateFontsTexture();
    // (this gets a bit more complicated, see example app for full reference)
    // ImGui_ImplVulkan_CreateFontsTexture(YOUR_COMMAND_BUFFER);
    // (your code submit a queue)
    //ImGui_ImplVulkan_DestroyFontUploadObjects(); //TODO JS: destroy queue
}

