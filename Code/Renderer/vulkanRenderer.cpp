
#include "vulkanRenderer.h"
#include<glm/gtc/quaternion.hpp>
#undef main
#define SDL_MAIN_HANDLED 
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <span>


#include "bufferCreation.h"
#include "CommandPoolManager.h"
#include "gpu-data-structs.h"
#include "meshData.h"
#include "../Scene/SceneObjectData.h"
#include "Vertex.h"
#include "../../ImageLibraryImplementations.h"
#include "../General/Array.h"
#include "../General/Memory.h"
#include "Shaders/ShaderLoading.h"
#include "textureCreation.h"
#include "TextureData.h"
#include "VkBootstrap.h"
#include "vulkan-utilities.h"
#include "../Scene/SceneObjectData.h"
#include "VulkanIncludes/VulkanMemory.h"

int SHADER_MODE;
VkSurfaceKHR surface;

#pragma region vkb setup
vkb::Instance GET_INSTANCE()
{
    vkb::InstanceBuilder instance_builder;
    auto instanceBuilderResult = instance_builder
                                  // .request_validation_layers()
                                 .use_default_debug_messenger()
                                 .require_api_version(1, 3, 240)
                                 .build();
    if (!instanceBuilderResult)
    {
        std::cerr << "Failed to create Vulkan instance. Error: " << instanceBuilderResult.error().message() << "\n";
        while(true);
    }

    return instanceBuilderResult.value();
}

bool HAS_HOST_IMAGE_COPY;

vkb::PhysicalDevice GET_GPU(vkb::Instance instance)
{

    const std::vector<const char*> deviceExtensions = {
         VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_MAINTENANCE_4_EXTENSION_NAME, VK_KHR_MAINTENANCE_3_EXTENSION_NAME, VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME, VK_EXT_MUTABLE_DESCRIPTOR_TYPE_EXTENSION_NAME,
        //for image copy:
        // VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME, VK_KHR_COPY_COMMANDS_2_EXTENSION_NAME, VK_KHR_FORMAT_FEATURE_FLAGS_2_EXTENSION_NAME,
    };
    VkPhysicalDeviceFeatures features{};
    VkPhysicalDeviceVulkan11Features features11{};
    VkPhysicalDeviceVulkan12Features features12{};
    VkPhysicalDeviceVulkan13Features features13{};
    features12.descriptorIndexing = VK_TRUE;
    features12.runtimeDescriptorArray = VK_TRUE;
    features12.descriptorBindingPartiallyBound = VK_TRUE;
    features13.dynamicRendering = VK_TRUE;
    features.wideLines = VK_TRUE;

   
    vkb::PhysicalDeviceSelector phys_device_selector(instance);
    auto physicalDeviceBuilderResult = phys_device_selector
                                       .set_minimum_version(1, 3)
                                       .set_surface(surface)
                                       .require_separate_transfer_queue()
                                       .set_required_features_12(features12)
    // .set_required_features({.})
    .set_required_features_13(features13)
                                       //NOTE: Not supporting gpus without dedicated queue 
                                       .require_separate_compute_queue()
                                       .add_required_extensions(deviceExtensions)
                                       .select();


    if (!physicalDeviceBuilderResult)
    {
        std::cerr << ("Failed to create Physical Device ")  << physicalDeviceBuilderResult.error().message();
        exit(1);
    }
    
    HAS_HOST_IMAGE_COPY = physicalDeviceBuilderResult.value().enable_extension_if_present(VK_EXT_HOST_IMAGE_COPY_EXTENSION_NAME);

    auto a =physicalDeviceBuilderResult.value().get_extensions();
    for(auto& v : a)
    {
        std::cerr << v << "\n";
    }
    return physicalDeviceBuilderResult.value();
}

vkb::Device GET_DEVICE(vkb::PhysicalDevice gpu)
{
    vkb::DeviceBuilder device_builder{gpu};

      auto devicebuilderResult =  device_builder.build();

    if (!devicebuilderResult)
    {
        std::cerr << ("Failed to create Virtual Device");
        exit(1);
    }

    return devicebuilderResult.value();
}

#pragma endregion



std::vector<unsigned int> pastTimes;
unsigned int averageCbTime;

unsigned int frames;

unsigned int MAX_TEXTURES = 30;

//TODO JS: move
struct gpuvertex
{
    alignas(16) glm::vec4 pos;
    alignas(16) glm::vec4 texCoord;
    alignas(16) glm::vec4 normal;
    alignas(16) glm::vec4 tangent;
};

//TODO JS: move
struct gpulight
{
    alignas(16) glm::vec4 pos_xyz_range_a;
    alignas(16) glm::vec4 color_xyz_intensity_a;
    alignas(16) glm::vec4 pointOrSpot_x_dir_yza;
    // alignas(16) glm::mat4 matrixViewProjection;
    alignas(16) glm::vec4 matrixIndex_matrixCount_padding_padding; // currently only used by point
};




HelloTriangleApplication::HelloTriangleApplication()
{
    initWindow();
    initVulkan();
    pastTimes.resize(9999);
    mainLoop();
    cleanup();
}


void HelloTriangleApplication::initWindow()
{
    // We initialize SDL and create a window with it. 
    SDL_Init(SDL_INIT_VIDEO);
    auto window_flags = SDL_WINDOW_VULKAN;

    //create blank SDL window for our application
    _window = SDL_CreateWindow(
        "Vulkan Engine", //window title
        SDL_WINDOWPOS_UNDEFINED, //window position x (don't care)
        SDL_WINDOWPOS_UNDEFINED, //window position y (don't care)
        WIDTH, //window width in pixels
        HEIGHT, //window height in pixels
        window_flags
    );
}

TextureData cube_irradiance;
TextureData cube_specular;

vkb::Instance vkb_instance;
vkb::PhysicalDevice vkb_physicalDevice;
vkb::Device vkb_device;
vkb::Swapchain vkb_swapchain;

void SET_UP_SCENE(HelloTriangleApplication* app);

RendererHandles HelloTriangleApplication::getHandles()
{
    return RendererHandles{physicalDevice, device, &commandPoolmanager, allocator, &rendererArena, &perFrameArenas[currentFrame], HAS_HOST_IMAGE_COPY};
}


//TODO JS: Eventually, these should change per frame
//TODO JS: I think I would create the views up front, and then swap them in and out at bind time 
void HelloTriangleApplication::updateShadowImageViews(int frame )
{
    int i = frame;
       
    shadowMapRenderingImageViews[i] =  MemoryArena::AllocSpan<VkImageView>(&rendererArena, MAX_SHADOWMAPS);
    for (int j = 0; j < MAX_SHADOWMAPS; j++)
    {
        shadowMapRenderingImageViews[i][j] = TextureUtilities::createImageView(
            device, shadowImages[i], shadowFormat,
            VK_IMAGE_ASPECT_DEPTH_BIT,
            (VkImageViewType)  VK_IMAGE_TYPE_2D,
            1,
            1, 
            j);
    }

    shadowMapSamplingImageViews[i] =  MemoryArena::AllocSpan<VkImageView>(&rendererArena, MAX_SHADOWCASTERS);
    for (int j = 0; j < MAX_SHADOWCASTERS; j++)
    {
        if (j < scene->shadowCasterCount())
        {
            VkImageViewType type = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
            int ct = scene->lightshadowMapCount[j];
            if (scene->lightTypes[j] == LIGHT_POINT)
            {
                type = VK_IMAGE_VIEW_TYPE_CUBE;
            }
           
            shadowMapSamplingImageViews[i][j] = TextureUtilities::createImageView(
                device, shadowImages[i], shadowFormat,
                VK_IMAGE_ASPECT_DEPTH_BIT,
                (VkImageViewType)  type,
                1,
               
                ct, 
                scene->getShadowDataIndex(j));
        }
        else
        {
            shadowMapSamplingImageViews[i][j] = shadowMapRenderingImageViews[i][j];
        }
    }
}



void HelloTriangleApplication::initVulkan()
{
    this->rendererArena = {};
    MemoryArena::initialize(&rendererArena, 1000000 * 10); // 10mb

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        MemoryArena::initialize(&perFrameArenas[i], 1000000 * 10); // 10mb each //TODO JS: Could be much smaller if I had a separate arena for file loading
    }
    FramesInFlightData.resize(MAX_FRAMES_IN_FLIGHT);
    //Get instance
    vkb_instance = GET_INSTANCE();
    instance = vkb_instance.instance;
    debugMessenger = vkb_instance.debug_messenger;

    //Get surface
    SDL_bool err = SDL_Vulkan_CreateSurface(_window, instance, &surface);
    if (!err)
    {
        std::cerr << "Failed to create SDL surface";
        exit(1);
    }

    SDL_Vulkan_GetDrawableSize(_window, &WIDTH, &HEIGHT);

    //Get physical device
    vkb_physicalDevice = GET_GPU(vkb_instance);
    physicalDevice = vkb_physicalDevice.physical_device;

    //Get logical device
    vkb_device = GET_DEVICE(vkb_physicalDevice);
    device = vkb_device.device;


   

    // vkCopyImageToMemoryEXT = (PFN_vkCopyImageToMemoryEXT)VkGetCopy(device, "vkCopyImageToMemoryEXT");
    allocator = VulkanMemory::GetAllocator(device, physicalDevice, instance);
    
    //Get queues and queue families and command pools
    commandPoolmanager = CommandPoolManager(vkb_device);

    //Swapchain
    vkb::SwapchainBuilder swapchain_builder{vkb_device};

    vkb_swapchain = swapchain_builder
                    .use_default_format_selection()
                    .set_desired_present_mode(VK_PRESENT_MODE_MAILBOX_KHR)
                    .set_desired_extent(WIDTH, HEIGHT)
                    .build()
                    .value();

    swapChain = vkb_swapchain.swapchain;
    swapChainImages = vkb_swapchain.get_images().value();
    swapChainImageViews = vkb_swapchain.get_image_views().value();
    swapChainExtent = vkb_swapchain.extent;
    swapChainColorFormat = vkb_swapchain.image_format;

    uint32_t shadowmapsize = SHADOW_MAP_SIZE;
    shadowFormat = VK_FORMAT_D32_SFLOAT;
    //Create shadow image(s) -- TODO JS: don't do this every frame
    shadowImages.resize(MAX_FRAMES_IN_FLIGHT);
    shadowSamplers.resize(MAX_FRAMES_IN_FLIGHT);
    shadowMemory.resize(MAX_FRAMES_IN_FLIGHT);
    shadowMapRenderingImageViews = MemoryArena::AllocSpan<std::span<VkImageView>>(&rendererArena, MAX_FRAMES_IN_FLIGHT); 
    shadowMapSamplingImageViews = MemoryArena::AllocSpan<std::span<VkImageView>>(&rendererArena, MAX_FRAMES_IN_FLIGHT); 

    createDepthResources();

    //Load shaders


    shaderLoader = new ShaderLoader(device);
    shaderLoader->AddShader("triangle", L"./Shaders/Shader1.hlsl");
    shaderLoader->AddShader("triangle_alt", L"./Shaders/shader2.hlsl");
    shaderLoader->AddShader("shadow", L"./Shaders/bindlessShadow.hlsl");
    shaderLoader->AddShader("lines", L"./Shaders/lines.hlsl");

    //Command buffer stuff
    createCommandBuffers();

    //Initialize scene
    camera.extent = {swapChainExtent.width, swapChainExtent.height};
    scene = MemoryArena::Alloc<Scene>(&rendererArena);
    InitializeScene(&rendererArena, scene);
    SET_UP_SCENE(this);

    perLightShadowData = MemoryArena::AllocSpan<std::span<PerShadowData>>(&rendererArena, scene->lightCount);

   
    //shadows
    //TODO JS: Will radically simplify things if I enforce point lights being first in light list?
    for(int i = 0; i < MAX_FRAMES_IN_FLIGHT ; i++)
    {
        TextureUtilities::createImage(getHandles(),shadowmapsize, shadowmapsize,shadowFormat,VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
               VK_IMAGE_USAGE_SAMPLED_BIT,0,shadowImages[i],shadowMemory[i],1, MAX_SHADOWMAPS, true);
        TextureData::createTextureSampler(&shadowSamplers[i], getHandles(), VK_SAMPLER_ADDRESS_MODE_REPEAT, 0, 1, true);
        
        updateShadowImageViews(i);
    }
       

    

    //Initialize scene-ish objects we don't have a place for yet 
    cubemaplut_utilitytexture_index = scene->AddUtilityTexture(
        TextureData(getHandles(), "textures/outputLUT.png", TextureData::DATA_DONT_COMPRESS));
    cube_irradiance = TextureData(getHandles(), "textures/output_cubemap2_diff8.ktx2", TextureData::TextureType::CUBE);
    cube_specular = TextureData(getHandles(), "textures/output_cubemap2_spec8.ktx2", TextureData::TextureType::CUBE);

    
    descriptorsetLayoutsData = PipelineDataObject(getHandles(), scene);


    //TODO JS: Make pipelines belong to the perPipelineLayout members
    createGraphicsPipeline("triangle_alt",  &descriptorsetLayoutsData);
    createGraphicsPipeline("triangle",  &descriptorsetLayoutsData);
    //TODO JS: separate shadow layout?
    createGraphicsPipeline("shadow",  &descriptorsetLayoutsData, true);

    createGraphicsPipeline("lines",  &descriptorsetLayoutsData, false, true);
    
    createDescriptorSetPool(getHandles(), &descriptorPool);

    descriptorsetLayoutsData.createDescriptorSetsOpaque(descriptorPool, MAX_FRAMES_IN_FLIGHT);
    descriptorsetLayoutsData.createDescriptorSetsShadow(descriptorPool, MAX_FRAMES_IN_FLIGHT);

    createUniformBuffers();
    createSyncObjects();

    //TODO JS: Move... Run when meshes change?
    populateMeshBuffers();
}

void HelloTriangleApplication::populateMeshBuffers()
{
    uint32_t vertCt = 0;
    for (int i = 0; i < scene->meshCount; i++)
    {
        vertCt += scene->backing_meshes[i].vertcount;
    }

    //TODO JS: to ring buffer?
    auto gpuVerts = MemoryArena::AllocSpan<gpuvertex>(getHandles().perframeArena, vertCt);

    uint32_t vert = 0;
    for (int j = 0; j < scene->meshCount; j++)
    {
        MeshData mesh = scene->backing_meshes[j];
        for (int i = 0; i < mesh.indices.size(); i++)
        {
            glm::vec4 pos = mesh.vertices[mesh.indices[i]].pos;
            glm::vec4 col = mesh.vertices[mesh.indices[i]].color;
            glm::vec4 uv = mesh.vertices[mesh.indices[i]].texCoord;
            glm::vec4 norm = mesh.vertices[mesh.indices[i]].normal;
            glm::vec4 tangent = mesh.vertices[mesh.indices[i]].tangent;
              gpuVerts[vert++] = {
                glm::vec4(pos.x, pos.y, pos.z, 1), uv, norm, glm::vec4(tangent.x, tangent.y, tangent.z, tangent.w)
            };
        
        }
    }
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        FramesInFlightData[i].meshBuffers.updateMappedMemory(gpuVerts.data(), sizeof(gpuvertex) * scene->getVertexCount());
    }
}

#pragma region descriptor sets

//TODO JS: Move?
//TODO JS: https://gpuopen-librariesandsdks.github.io/VulkanMemoryAllocator/html/usage_patterns.html advanced
void HelloTriangleApplication::createUniformBuffers()
{
    VkDeviceSize globalsSize = sizeof(ShaderGlobals);
    VkDeviceSize ubosSize = sizeof(UniformBufferObject) * scene->objectsCount();
    VkDeviceSize vertsSize = sizeof(gpuvertex) * scene->getVertexCount();
    VkDeviceSize lightdataSize = sizeof(gpulight) * scene->lightCount;
    VkDeviceSize shadowDataSize = sizeof(PerShadowData) * scene->lightCount * 10; //times six is plenty right?

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        FramesInFlightData[i].perLightShadowShaderGlobalsBuffer.resize(MAX_SHADOWCASTERS);
        FramesInFlightData[i].perLightShadowShaderGlobalsMapped.resize(MAX_SHADOWCASTERS);
        FramesInFlightData[i].perLightShadowShaderGlobalsMemory.resize(MAX_SHADOWCASTERS);

        for (size_t j = 0; j < MAX_SHADOWCASTERS ; j++)
        {
            FramesInFlightData[i].perLightShadowShaderGlobalsBuffer[j].size = globalsSize;
            FramesInFlightData[i].perLightShadowShaderGlobalsMapped[j] = BufferUtilities::createDynamicBuffer(
                getHandles(), globalsSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                &FramesInFlightData[i].perLightShadowShaderGlobalsMemory[j],
                FramesInFlightData[i].perLightShadowShaderGlobalsBuffer[j].data);
        }
        
        FramesInFlightData[i].opaqueShaderGlobalsBuffer.size = globalsSize;
        FramesInFlightData[i].opaqueShaderGlobalsBuffer.mapped = BufferUtilities::createDynamicBuffer(
            getHandles(), globalsSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            &FramesInFlightData[i].opaqueShaderGlobalsMemory,
            FramesInFlightData[i].opaqueShaderGlobalsBuffer.data);
        
        FramesInFlightData[i].uniformBuffers.size = ubosSize;
        FramesInFlightData[i].uniformBuffers.mapped = BufferUtilities::createDynamicBuffer(
            getHandles(), ubosSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            &FramesInFlightData[i].uniformBuffersMemory,
            FramesInFlightData[i].uniformBuffers.data);
        
        FramesInFlightData[i].meshBuffers.size = vertsSize;
        FramesInFlightData[i].meshBuffers.mapped = BufferUtilities::createDynamicBuffer(
            getHandles(), vertsSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            &FramesInFlightData[i].meshBuffersMemory,
            FramesInFlightData[i].meshBuffers.data);

        FramesInFlightData[i].lightBuffers.size = lightdataSize;
        FramesInFlightData[i].lightBuffers.mapped = BufferUtilities::createDynamicBuffer(
            getHandles(), lightdataSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            &FramesInFlightData[i].lightBuffersMemory,
            FramesInFlightData[i].lightBuffers.data);

        FramesInFlightData[i].shadowDataBuffers.size = shadowDataSize;
        FramesInFlightData[i].shadowDataBuffers.mapped = BufferUtilities::createDynamicBuffer(
            getHandles(), shadowDataSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            &FramesInFlightData[i].shadowDataBuffersMemory,
            FramesInFlightData[i].shadowDataBuffers.data);

    }
}

#pragma endregion

#pragma region descriptorsets
void HelloTriangleApplication::createDescriptorSetPool(RendererHandles handles, VkDescriptorPool* pool)
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
    VK_CHECK( vkCreateDescriptorPool(handles.device, &pool_info, nullptr, pool));
    
}


//TODO JS: move
VkDescriptorImageInfo imageInfoFromImageData(
    TextureData texture)
{
    assert( &texture.textureImageView != VK_NULL_HANDLE);
    return 
        VkDescriptorImageInfo{
           .sampler = texture.textureSampler,  .imageView = texture.textureImageView, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
}


//TODO JS: this is replacing a function that used to be in scene -- need to feed its arguments from scene's backing arrays
std::span<VkDescriptorImageInfo> getBindlessTextureInfos(MemoryArena::memoryArena* allocator, std::span<TextureData> diffuse, std::span<TextureData> spec, std::span<TextureData> norm, std::span<TextureData> utility)
{
    assert(spec.size() == diffuse.size());
    assert(spec.size() == norm.size());
    //TODO JS: Don't do this every frame
    Array<VkDescriptorImageInfo> imageInfos = MemoryArena::AllocSpan<VkDescriptorImageInfo>(allocator, (diffuse.size() * 3) + utility.size());
    // std::vector<VkDescriptorImageInfo> samplerInfos;
    //Material textures
    for (int texture_i = 0; texture_i < diffuse.size(); texture_i++)
    {
        auto imageInfo = imageInfoFromImageData(
            diffuse[texture_i]);
        imageInfos.push_back(imageInfo);
        // samplerInfos.push_back(imageInfo);

        auto imageInfo2 = imageInfoFromImageData(
            spec[texture_i]);
        imageInfos.push_back(imageInfo2);
        // samplerInfos.push_back(imageInfo2);

        auto imageInfo3 = imageInfoFromImageData(
            norm[texture_i]);
        imageInfos.push_back(imageInfo3);
        // samplerInfos.push_back(imageInfo3);
    }

    for (int texture_i = 0; texture_i < utility.size(); texture_i++)
    {
        auto imageInfo = imageInfoFromImageData(
            utility[texture_i]);
        imageInfos.push_back(imageInfo);
        // samplerInfos.push_back(imageInfo);
    }

    return std::span(imageInfos.data, imageInfos.ct);
}


void HelloTriangleApplication::updateOpaqueDescriptorSets(RendererHandles handles, VkDescriptorPool pool, PipelineDataObject* layoutData)
{

    //Get data
    auto imageInfos = getBindlessTextureInfos(
        handles.perframeArena,
        std::span(scene->backing_diffuse_textures.data, scene->backing_diffuse_textures.ct ),
        std::span(scene->backing_specular_textures.data, scene->backing_specular_textures.ct ),
        std::span(scene->backing_normal_textures.data, scene->backing_normal_textures.ct ),
        std::span(scene->backing_utility_textures.data, scene->backing_utility_textures.ct ));
    auto [cubeImageInfos, cubeSamplerInfos] = DescriptorSets::ImageInfoFromImageDataVec({
        cube_irradiance, cube_specular});


    int pointCount = 0;

    //TODO JS: !!!! Fix this 
    VkDescriptorImageInfo shadows[MAX_SHADOWMAPS] = {};
    for(int i = 0; i < MAX_SHADOWMAPS; i++)
    {
        VkImageView view {};
        if (i < scene->shadowCasterCount())
        {
            bool point = scene->lightTypes[i] == LIGHT_POINT;
            int idx = scene->getShadowDataIndex(i);
            view =  shadowMapSamplingImageViews[currentFrame][ i] ;
        }

        //Fill out the remaining leftover shadow bindings with something -- using the other shadow view is arbitrary
        //TODO JS: Should have a "default" view maybe? 
        else
        {
            view = shadowMapRenderingImageViews[currentFrame][0];
        }
        
        assert( &view != VK_NULL_HANDLE);
        VkDescriptorImageInfo shadowInfo = {
            //TODO JS: make sure the right stuff is in currentFrame[0]
            .imageView = view, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };
        shadows[i] = shadowInfo;
    }

    VkDescriptorImageInfo shadowSamplerInfo = {
        .sampler  = shadowSamplers[currentFrame], .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

   
    
    VkDescriptorBufferInfo meshBufferinfo = FramesInFlightData[currentFrame].meshBuffers.getBufferInfo();
    VkDescriptorBufferInfo lightbufferinfo = FramesInFlightData[currentFrame].lightBuffers.getBufferInfo();
    VkDescriptorBufferInfo uniformbufferinfo = FramesInFlightData[currentFrame].uniformBuffers.getBufferInfo();
    VkDescriptorBufferInfo shaderglobalsinfo = FramesInFlightData[currentFrame].opaqueShaderGlobalsBuffer.getBufferInfo();
    
      VkDescriptorBufferInfo shadowBuffersInfo = FramesInFlightData[currentFrame].shadowDataBuffers.getBufferInfo();

    

    std::vector<descriptorUpdateData> descriptorUpdates;
    //Update descriptor sets with data
 
    descriptorUpdates.push_back({VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &shaderglobalsinfo});

    descriptorUpdates.push_back({VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, imageInfos.data(),  (uint32_t)imageInfos.size()});
    descriptorUpdates.push_back({VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, cubeImageInfos.data(), (uint32_t)cubeImageInfos.size()});
    descriptorUpdates.push_back({VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, &shadows,  (uint32_t)MAX_SHADOWMAPS}); //shadows

    descriptorUpdates.push_back({VK_DESCRIPTOR_TYPE_SAMPLER, imageInfos.data(), (uint32_t)imageInfos.size()});
    descriptorUpdates.push_back({VK_DESCRIPTOR_TYPE_SAMPLER, cubeSamplerInfos.data(), (uint32_t)cubeSamplerInfos.size()});
    descriptorUpdates.push_back({VK_DESCRIPTOR_TYPE_SAMPLER, &shadowSamplerInfo, (uint32_t)1}); //shadows

    descriptorUpdates.push_back({VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &meshBufferinfo});
    descriptorUpdates.push_back({VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &lightbufferinfo});
    descriptorUpdates.push_back({VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &uniformbufferinfo});
    
    descriptorUpdates.push_back({VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &shadowBuffersInfo});
    

    layoutData->updateOpaqueDescriptorSets(descriptorUpdates, currentFrame);
}


//TODO JS: Need to use separate descriptors  
//TODO JS: Probably want to duplicate less code
void HelloTriangleApplication::updateShadowDescriptorSets(RendererHandles handles,  VkDescriptorPool pool, PipelineDataObject* layoutData, uint32_t shadowIndex)
{

    //Get data
    VkDescriptorBufferInfo meshBufferinfo = FramesInFlightData[currentFrame].meshBuffers.getBufferInfo();
    VkDescriptorBufferInfo uniformbufferinfo = FramesInFlightData[currentFrame].uniformBuffers.getBufferInfo();
    VkDescriptorBufferInfo shaderglobalsinfo = FramesInFlightData[currentFrame].perLightShadowShaderGlobalsBuffer[shadowIndex].getBufferInfo();

    VkDescriptorBufferInfo lightbufferinfo = FramesInFlightData[currentFrame].lightBuffers.getBufferInfo();
    VkDescriptorBufferInfo shadowBuffersInfo = FramesInFlightData[currentFrame].shadowDataBuffers.getBufferInfo();

    std::vector<descriptorUpdateData> descriptorUpdates;
    //Update descriptor sets with data
 
    descriptorUpdates.push_back({VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &shaderglobalsinfo});
    descriptorUpdates.push_back({VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &meshBufferinfo});
    descriptorUpdates.push_back({VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &lightbufferinfo});
    descriptorUpdates.push_back({VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &uniformbufferinfo});
    descriptorUpdates.push_back({VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &shadowBuffersInfo});

    layoutData->
    updateShadowDescriptorSets(descriptorUpdates, currentFrame);

    
}
#pragma endregion


void HelloTriangleApplication::createSyncObjects()
{
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    for(int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
            if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &FramesInFlightData[i].imageAvailableSemaphores) != VK_SUCCESS ||
                vkCreateSemaphore(device, &semaphoreInfo, nullptr, &FramesInFlightData[i].renderFinishedSemaphores) != VK_SUCCESS ||
                vkCreateFence(device, &fenceInfo, nullptr, &FramesInFlightData[i].inFlightFences) != VK_SUCCESS)
            {
                printf("failed to create synchronization objects for a frame!");
                assert(false);
            }
    }
    for(int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &FramesInFlightData[i].shadowAvailableSemaphores) != VK_SUCCESS ||
            vkCreateSemaphore(device, &semaphoreInfo, nullptr, &FramesInFlightData[i].shadowFinishedSemaphores) != VK_SUCCESS)
        {
            printf("failed to create synchronization objects for a shadow pass!");
            assert(false);
        }
    }
    for(int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &FramesInFlightData[i].swapchaintransitionedOutSemaphores) != VK_SUCCESS ||
            vkCreateSemaphore(device, &semaphoreInfo, nullptr, &FramesInFlightData[i].swapchaintransitionedInSemaphores) != VK_SUCCESS)
        {
            printf("failed to create synchronization objects for swapchain!");
            assert(false);
        }
    }

    for(int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        //TODO JS: for each
        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &FramesInFlightData[i].shadowtransitionedOutSemaphores) != VK_SUCCESS ||
            vkCreateSemaphore(device, &semaphoreInfo, nullptr, &FramesInFlightData[i].shadowtransitionedInSemaphores) != VK_SUCCESS)
        {
            printf("failed to create synchronization objects for swapchain!");
            assert(false);
        }
    }
   

}

//TODO JS: none of this belongs here except for actually submitting the updates
#pragma region per-frame updates 


glm::quat OrientationFromYawPitch(glm::vec2 yawPitch )
{
    glm::quat yawQuat = glm::angleAxis(glm::radians(yawPitch.x), glm::vec3(0, -1, 0));
    glm::quat pitchQUat = glm::angleAxis(( glm::radians(-yawPitch.y)), glm::vec3(1, 0, 0));
    return yawQuat * pitchQUat;
}

Transform getCameraTransform(HelloTriangleApplication::cameraData camera)
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
std::vector<linePair> debugLines;
//TODO JS: this sucks!
void HelloTriangleApplication::updateCamera(inputData input)
{
    camera.eyeRotation += (input.mouseRot *  30000.0f *  deltaTime);  // 30000 degrees per full screen rotation per second
    if(camera.eyeRotation.y > 89.0f)
        camera.eyeRotation.y = 89.0f;
    if(camera.eyeRotation.y < -89.0f)
        camera.eyeRotation.y = -89.0f;

    glm::quat Orientation = OrientationFromYawPitch(camera.eyeRotation);
 
    glm::quat forwardQuat = Orientation * glm::quat(0, 0, 0, -1) * glm::conjugate(Orientation);
    glm::vec3 UP = glm::vec3(0,1,0);
    glm::vec3 Front = { forwardQuat.x, forwardQuat.y, forwardQuat.z };
    glm::vec3 RIGHT = glm::normalize(glm::cross(Front, UP));
    
    glm::vec3 translateFWD = Front * input.keyboardMove.y;
    glm::vec3 translateSIDE = RIGHT * input.keyboardMove.x;

    camera.eyePos +=  (translateFWD + translateSIDE) * deltaTime;
}

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

struct viewProj
{
    glm::mat4 view;
    glm::mat4 proj;
};

viewProj viewProjFromCamera( HelloTriangleApplication::cameraData camera)
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

std::span<PerShadowData> calculateLightMatrix(MemoryArena::memoryArena* allocator, HelloTriangleApplication::cameraData cam, glm::vec3 lightPos, glm::vec3 spotDir, float spotRadius, lightType type)
{
    viewProj vp = viewProjFromCamera(cam);
    
    glm::vec3 dir = type ==  LIGHT_SPOT ? spotDir : glm::normalize(lightPos);
    glm::vec3 up = glm::vec3( 0.0f, 1.0f,  0.0f);
    //offset directions that are invalid for lookat
    if (abs(up) == abs(dir))
    {
        dir += glm::vec3(0.00001);
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
            outputSpan = MemoryArena::AllocSpan<PerShadowData>(allocator, 4 ); //TODO JS: cascades

            glm::mat4 invCam = glm::inverse(vp.proj * vp.view);

            glm::vec4 frustumCornersWorldSpace[8] = {};
            populateFrustumCornersForSpace(frustumCornersWorldSpace, invCam);

         
        
            debugDrawFrustum(frustumCornersWorldSpace);

            //TODO JS: direciton lights only currently
          

         

            float clipRange = cam.farPlane - cam.nearPlane;
            float minZ =  cam.nearPlane;
            float maxZ =  cam.nearPlane + clipRange;

            float range = maxZ - minZ;
            float ratio = maxZ / minZ;
         
            
            //cascades
            float cascadeSplits[4] = {};
            for (uint32_t i = 0; i < 4; i++) {
                float p = (i + 1) / static_cast<float>(4);
                float log = minZ * std::pow(ratio, p);
                float uniform = minZ + range * p;
                float d = 0.98f * (log - uniform) + uniform;
                cascadeSplits[i] = (d - cam.nearPlane) / clipRange;
            }

            for (int i = 0; i < 4; i++)
            {
                float splitDist = cascadeSplits[i];
                float lastSplitDist = i == 0 ? 0 : cascadeSplits[i-1]; 
                
                glm::vec3 frustumCornersCascadeSpace[8] = {};
                for(int j = 0; j < 8; j++)
                {
                    frustumCornersCascadeSpace[j] = glm::vec3(frustumCornersWorldSpace[j]);
                }
                for(int j = 0; j < 4; j++)
                {
                    glm::vec3 dist = frustumCornersCascadeSpace[j + 4] - frustumCornersCascadeSpace[j];
                    frustumCornersCascadeSpace[j + 4] = frustumCornersCascadeSpace[j] + (dist * splitDist);
                    frustumCornersCascadeSpace[j] = frustumCornersCascadeSpace[j] + (dist * lastSplitDist);
                }

                glm::vec3 frustumCenter = glm::vec3(0.0f);
                for (uint32_t j = 0; j < 8; j++) {
                    frustumCenter += frustumCornersCascadeSpace[j];
                }
                frustumCenter /= 8.0f;

                debugDrawCross(frustumCenter, 20, {0,0,1});
               

   
                float radius = 0.0f;
                for (uint32_t i = 0; i < 8; i++) {
                    float distance = glm::length(glm::vec3(frustumCornersCascadeSpace[i]) - frustumCenter);
                    radius = glm::max<float>(radius, distance);
                }
                
                radius = std::ceil(radius * 16.0f) / 16.0f;

                glm::vec3 maxExtents = glm::vec3(radius);
                glm::vec3 minExtents = -maxExtents;

                lightViewMatrix = glm::lookAt(glm::vec3(frustumCenter)  + (dir * (maxExtents.z)), frustumCenter, up);

                glm::mat4 lightOrthoMatrix = glm::ortho(minExtents.x, maxExtents.x, minExtents.y, maxExtents.y, 0.0f, maxExtents.z - minExtents.z);

                lightProjection =  lightOrthoMatrix;
                outputSpan[i] = {lightProjection * lightViewMatrix, (cam.nearPlane + splitDist * clipRange) * -1.0f};
                // OUTPUTPOSITION =  (cam.nearPlane + splitDist * clipRange) * -1.0f; //todo js
            }
            return  outputSpan.subspan(0,4);
        }
    case LIGHT_SPOT:
        {
            outputSpan = MemoryArena::AllocSpan<PerShadowData>(allocator, 1 ); 
            lightViewMatrix = glm::lookAt(lightPos, lightPos + dir, up);
            
            lightProjection = glm::perspective(glm::radians(spotRadius),
                                      1.0f, 0.1f,
                                      50.0f); //TODO BETTER FAR 
            outputSpan[0] = {lightProjection * lightViewMatrix, 0};
                                  
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
    
    outputSpan[0].shadowMatrix =  glm::rotate(rotMatrix, glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    outputSpan[0].shadowMatrix = lightProjection *   (glm::rotate(  outputSpan[0].shadowMatrix, glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)) * translation);
      rotMatrix = glm::mat4(1.0);
    outputSpan[1].shadowMatrix = glm::rotate(rotMatrix, glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    outputSpan[1].shadowMatrix = lightProjection *   (glm::rotate(outputSpan[1].shadowMatrix, glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)) * translation);
      rotMatrix = glm::mat4(1.0);
    outputSpan[2].shadowMatrix = lightProjection *  (glm::rotate(rotMatrix, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f)) * translation);
      rotMatrix = glm::mat4(1.0);
    outputSpan[3].shadowMatrix = lightProjection *  (glm::rotate(rotMatrix, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)) * translation);
      rotMatrix = glm::mat4(1.0);
    outputSpan[4].shadowMatrix = lightProjection *  (glm::rotate(rotMatrix, glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)) * translation);
      rotMatrix = glm::mat4(1.0);
    outputSpan[5].shadowMatrix = lightProjection *  (glm::rotate(rotMatrix, glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f)) * translation);
       rotMatrix = glm::mat4(1.0);   
        return  outputSpan;
        }


   

    
}


void updateGlobals(HelloTriangleApplication::cameraData camera, Scene* scene, int cubeMapLutIndex, dataBuffer globalsBuffer)
{
    ShaderGlobals globals;
    viewProj vp = viewProjFromCamera(camera);
    globals.view = vp.view;
    globals.proj = vp.proj;
    globals.viewPos = glm::vec4(camera.eyePos.x, camera.eyePos.y, camera.eyePos.z, 1);
    globals.lightcountx_modey_shadowcountz_padding_w = glm::vec4(scene->lightCount, SHADER_MODE, MAX_SHADOWCASTERS, 0);
    globals.cubemaplutidx_cubemaplutsampleridx_paddingzw = glm::vec4(
        scene->materialTextureCount() + cubeMapLutIndex,
        scene->materialTextureCount() + cubeMapLutIndex, 0, 0);
    
    globalsBuffer.updateMappedMemory(&globals, sizeof(ShaderGlobals));

}

void updateShadowData(MemoryArena::memoryArena* allocator, std::span<std::span<PerShadowData>> perLightShadowData, Scene* scene, HelloTriangleApplication::cameraData camera)
{
    for(int i =0; i <scene->lightCount; i++)
    {
        
        perLightShadowData[i] =  calculateLightMatrix(allocator, camera,
             (scene->lightposandradius[i]), scene->lightDir[i], scene->lightposandradius[i].w, static_cast<lightType>(scene->lightTypes[i]));
    }
}


void HelloTriangleApplication::updatePerFrameBuffers(uint32_t currentFrame, Array<glm::mat4> models
                                                   )
{
    //TODO JS: to ring buffer?
    auto tempArena = getHandles().perframeArena;

    updateGlobals(camera, scene, cubemaplut_utilitytexture_index, FramesInFlightData[currentFrame].opaqueShaderGlobalsBuffer);

    //Lights
    auto lights = MemoryArena::AllocSpan<gpulight>(tempArena, scene->lightCount);
    Array<PerShadowData> flattenedPerShadowData = Array(MemoryArena::AllocSpan<PerShadowData>(&perFrameArenas[currentFrame],FramesInFlightData[currentFrame].shadowDataBuffers.size / sizeof(PerShadowData)));
    for (int i = 0; i < perLightShadowData.size(); i++)
    {
        std::span<PerShadowData> lightsShadowData = perLightShadowData[i];
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
    FramesInFlightData[currentFrame].lightBuffers.updateMappedMemory(lights.data(), sizeof(gpulight) * lights.size());//TODO JS: could probably bump the index in the shader too on the point path rather than rebinding. 
    FramesInFlightData[currentFrame].shadowDataBuffers.updateMappedMemory(flattenedPerShadowData.data, flattenedPerShadowData.capacity_bytes()); 


    //Ubos
    auto ubos = MemoryArena::AllocSpan<UniformBufferObject>(tempArena,scene->objectsCount());

    for (int i = 0; i < ubos.size(); i++)
    {
        glm::mat4* model = &models[i];
        ubos[i].model = models[i];
        ubos[i].Normal = transpose(inverse(glm::mat3(*model)));
    }
    
    FramesInFlightData[currentFrame].uniformBuffers.updateMappedMemory(ubos.data(), sizeof(UniformBufferObject) *scene->objectsCount());

    
}


#pragma endregion
#pragma region draw 
void HelloTriangleApplication::recordCommandBufferShadowPass(VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
     uint32_t shadowSize = SHADOW_MAP_SIZE; //TODO JS: make const
     VkCommandBufferBeginInfo beginInfo{};
     beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
     beginInfo.flags = 0; // Optional
     beginInfo.pInheritanceInfo = nullptr; // Optional
     //Transition swapchain for rendering
    
     VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));


    uint32_t SHADOW_INDEX = 0; //TODO JS: loop over shadowcasters
    updateShadowDescriptorSets(getHandles(), descriptorPool, &descriptorsetLayoutsData,SHADOW_INDEX);
    descriptorsetLayoutsData.bindToCommandBufferShadow(commandBuffer, currentFrame);
    VkPipelineLayout layout = descriptorsetLayoutsData.getLayoutShadow();

    
    int shadowCasterCount = min(scene->lightCount, MAX_SHADOWCASTERS);
    //Should I do separate command buffers per shadow index?

    int meshct = scene->objectsCount();
    

    std::span<int> meshIndices = MemoryArena::AllocSpan<int>(&perFrameArenas[currentFrame], meshct);

      
    for(int i =0; i < meshct; i++)
    {
        meshIndices[i] = i;
    }



    
    for(int i = 0; i < shadowCasterCount; i ++)
    {
        //TODO JS: next step -- need to get the 6 different point light matrices in. Or transform the one 6 times?
        scene->OrderedMeshes(scene->lightTypes[i] == LIGHT_DIR ? glm::vec3(scene->lightposandradius[i]) * 9999.0f : glm::vec3(scene->lightposandradius[i]), meshIndices, false);
        int shadowIndex = i;
        int shadowCasterOffset = scene->getShadowDataIndex(i);
        lightType type = (lightType)scene->lightTypes[i];
        int subpasses = type == lightType::LIGHT_POINT ? 6 : type == LIGHT_DIR ? CASCADE_CT : 1; //TODO JS: look up how many a light should have per light
        for (int j = 0; j < subpasses; j++)
        {
            int shadowMapIndex = shadowCasterOffset + j;
            const VkRenderingAttachmentInfoKHR dynamicRenderingDepthAttatchment {
                .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
                .imageView = shadowMapRenderingImageViews[imageIndex][shadowMapIndex],
                .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .clearValue =  {1.0f, 0}
            };

            VkRenderingInfo renderPassInfo{};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;

            renderPassInfo.renderArea.extent = {shadowSize , shadowSize}; //TODO JS: Shadow size
            renderPassInfo.renderArea.offset = {0, 0};
         
            renderPassInfo.layerCount =1;
            renderPassInfo.colorAttachmentCount = 0;
            renderPassInfo.pColorAttachments = VK_NULL_HANDLE;
            renderPassInfo.pDepthAttachment = &dynamicRenderingDepthAttatchment;

            vkCmdBeginRendering(commandBuffer, &renderPassInfo);
   
            VkViewport viewport{};
            viewport.x = 0.0f;
            viewport.y = 0.0f;
            viewport.width = static_cast<float>(shadowSize );
            viewport.height = static_cast<float>(shadowSize );
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

            VkRect2D scissor{};
            scissor.offset = {0, 0};
            scissor.extent = {shadowSize , shadowSize };
            vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

            //*************
            //************
            //** Descriptor Sets update and binding
            // This could change sometimes


            //TODO JS: dynamically set bias per shadow caster, especially for cascades
            float baseDepthBias = 6.0;
            float baseSLopeBias = 3.0;
            vkCmdSetDepthBias(
                        commandBuffer,
                        baseDepthBias,
                        0.0f,
                        baseSLopeBias);


     
        
            
            //TODO JS: Something other than hardcoded index 2 for shadow pipeline
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, descriptorsetLayoutsData.getPipeline(2));
            int meshct =  scene->objectsCount();
            for (int j = 0; j <meshct; j++)
            {
                int i = meshIndices[j];
                per_object_data constants;
                //Light count, vert offset, texture index, and object data index
                constants.indexInfo = glm::vec4(-1, (scene->objects.meshOffsets[i]),
                                                -1, i);
                constants.Indexinfo2 = glm::vec4(-1,-1,shadowMapIndex,shadowIndex);

                constants.materialprops = glm::vec4(-1, -1, 0, 0);

                vkCmdPushConstants(commandBuffer, layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                                   sizeof(per_object_data), &constants);

                vkCmdDraw(commandBuffer, static_cast<uint32_t>(scene->objects.meshes[i]->vertcount), 1, 0, 0);
            }
            vkCmdEndRendering(commandBuffer);
        }


        
    }   

     VK_CHECK(vkEndCommandBuffer(commandBuffer));
}

//command buffer to draw the frame 
void HelloTriangleApplication::recordCommandBufferOpaquePass(VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = 0; // Optional
    beginInfo.pInheritanceInfo = nullptr; // Optional
    //Transition swapchain for rendering
    
    
    VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

    
    const VkRenderingAttachmentInfoKHR dynamicRenderingColorAttatchment {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
        .imageView = swapChainImageViews[imageIndex],
        .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue =  {0.0f, 0.0f, 0.0f, 1.0f}
    };

    const VkRenderingAttachmentInfoKHR dynamicRenderingDepthAttatchment {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
        .imageView = depthImageView,
        .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue =  {1.0f, 0}
    };

    VkRenderingInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderPassInfo.renderArea.extent = swapChainExtent;
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.layerCount =1;
    renderPassInfo.colorAttachmentCount = 1;
    renderPassInfo.pColorAttachments = &dynamicRenderingColorAttatchment;
    renderPassInfo.pDepthAttachment = &dynamicRenderingDepthAttatchment;

    vkCmdBeginRendering(commandBuffer, &renderPassInfo);
  
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(swapChainExtent.width);
    viewport.height = static_cast<float>(swapChainExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = swapChainExtent;
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    uint32_t debugpipelineindexoverride = _selectedShader == 0 ? 0 : 1;
// #define DEBUG_SHADERS
    
    //*************
    //************
    //** Descriptor Sets update and binding
    // This could change sometimes
    updateOpaqueDescriptorSets(getHandles(), descriptorPool, &descriptorsetLayoutsData);
    descriptorsetLayoutsData.bindToCommandBufferOpaque(commandBuffer, currentFrame);
    VkPipelineLayout layout = descriptorsetLayoutsData.getLayoutOpaque();
    
    int meshct = scene->objectsCount();
    
    std::span<int> meshIndices = MemoryArena::AllocSpan<int>(&perFrameArenas[currentFrame], meshct);
    for(int i =0; i < meshct; i++)
    {
        meshIndices[i] = i;
    }

    scene->OrderedMeshes(camera.eyePos , meshIndices, false );
    
    int lastPipelineIndex = -1;
    for (int j = 0; j <meshct; j++)
    {
        int i = meshIndices[j];
        Material material = scene->objects.materials[i];
        int pipelineIndex =1 ;
#ifdef DEBUG_SHADERS
        pipelineIndex = debugpipelineindexoverride;
#endif
        if (pipelineIndex != lastPipelineIndex)
        {
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, descriptorsetLayoutsData.getPipeline(pipelineIndex));
        }
        
        lastPipelineIndex = pipelineIndex;
        
        per_object_data constants;
        //Light count, vert offset, texture index, and object data index
        constants.indexInfo = glm::vec4(material.backingTextureidx, (scene->objects.meshOffsets[i]),
                                        material.backingTextureidx, i);

        constants.materialprops = glm::vec4(material.roughness, scene->objects.materials[i].metallic, 0, 0);

        vkCmdPushConstants(commandBuffer, layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                           sizeof(per_object_data), &constants);

        vkCmdDraw(commandBuffer, static_cast<uint32_t>(scene->objects.meshes[i]->vertcount), 1, 0, 0);
    }

    //debug lines -- todo feed from somehwere
                                                                            //TODO JS: something other than hardcoded 3
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, descriptorsetLayoutsData.getPipeline(3));
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
                          sizeof(per_object_data), &constants);

        vkCmdDraw(commandBuffer, 2, 1, 0, 0);
    }

    vkCmdEndRendering(commandBuffer);

    VK_CHECK(vkEndCommandBuffer(commandBuffer));

    frames ++;
   
}
#pragma endregion

void HelloTriangleApplication::createCommandBuffers()
{
    for (int i = 0; i < FramesInFlightData.size(); i++)
    {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = commandPoolmanager.commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, &FramesInFlightData[i].opaqueCommandBuffers));
        VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, &FramesInFlightData[i].shadowCommandBuffers));
        VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, &FramesInFlightData[i].swapchainTransitionInCommandBuffer));
        VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, &FramesInFlightData[i].swapchainTransitionOutCommandBuffer));
        VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, &FramesInFlightData[i].shadowTransitionInCommandBuffer));
        VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, &FramesInFlightData[i].shadowTransitionOutCommandBuffer));
    }
}

bool HelloTriangleApplication::hasStencilComponent(VkFormat format)
{
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

#pragma region depth

void HelloTriangleApplication::createDepthResources()
{
    depthFormat = Capabilities::findDepthFormat(getHandles());

    TextureUtilities::createImage(getHandles(), swapChainExtent.width, swapChainExtent.height, depthFormat,
                                  VK_IMAGE_TILING_OPTIMAL,
                                  VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                  depthImage,
                                  depthImageMemory);
    depthImageView =
        TextureUtilities::createImageView(this->device, depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
}

#pragma endregion


void HelloTriangleApplication::createGraphicsPipeline(const char* shaderName, PipelineDataObject* descriptorsetdata, bool shadow, bool lines)
{
    VkPipeline newGraphicsPipeline; 
    auto shaders = shaderLoader->compiledShaders[shaderName];

    if (!shadow)
    descriptorsetdata->createGraphicsPipeline(shaders,&swapChainColorFormat, &depthFormat, false, true, true, lines);
    else
    {
        descriptorsetdata->createGraphicsPipeline(shaders,&swapChainColorFormat, &depthFormat,  true, false, true);
    } 

    auto val = shaderLoader->compiledShaders[shaderName];

    for (auto v : val)
    {
        vkDestroyShaderModule(device, v.module, nullptr);
    }

}


#pragma region perFrameUpdate


void HelloTriangleApplication::mainLoop()
{
    SDL_Event e;
    bool bQuit = false;

    float translateSpeed = 3.0;
    float mouseSpeed = 1.0;
    while (!bQuit)
    {
        this->T2 = SDL_GetTicks();
        uint32_t deltaTicks = this->T2 - this->T;
        this->deltaTime = deltaTicks * 0.001;
        this->T = SDL_GetTicks();



    
        
        //Handle events on queue
        while (SDL_PollEvent(&e) != 0)
        {
            //close the window when user clicks the X button or alt-f4s
            if (e.type == SDL_QUIT) bQuit = true;
            else if (e.type == SDL_KEYDOWN)
            {
                if (e.key.keysym.sym == SDLK_SPACE)
                {
                    _selectedShader += 1;
                    if (_selectedShader > 1)
                    {
                        _selectedShader = 0;
                    }
                }

                if (e.key.keysym.sym == SDLK_z)
                {
                    SHADER_MODE = SHADER_MODE ? 0 : 1; //debug toggle float we can bool off of on shader
                }
                
            }
          
        }

        glm::vec3 translate = glm::vec3(0);
        SDL_PumpEvents();
        const uint8_t* KeyboardState = SDL_GetKeyboardState(nullptr);
        translate.x = 0 -KeyboardState[SDL_SCANCODE_A] + KeyboardState[SDL_SCANCODE_D];
        translate.y = 0 +KeyboardState[SDL_SCANCODE_W] - KeyboardState[SDL_SCANCODE_S];
        translate *= translateSpeed;
        int x, y;

        const uint32_t MouseButtonState = SDL_GetRelativeMouseState(&x, &y);

      
        glm::vec3 mouseRot = glm::vec3(0);
        if (MouseButtonState & SDL_BUTTON_LMASK) // mouse down
        {
            float fx  = x / 1000.0f;
            float fy  = y / 1000.0f;
            mouseRot.x = fx;
            mouseRot.y = fy;
        }
        inputData input = {translate, mouseRot}; 
        //Temp placeholder for like, object loop
        updateCamera(input);
        UpdateRotations();
        scene->Update();
        updateShadowData(&perFrameArenas[currentFrame], perLightShadowData, scene, camera);
        drawFrame();
        debugLines.clear();
        auto t2 = SDL_GetTicks();
    }
    vkDeviceWaitIdle(device);
}

//Placeholder "gameplay" function
void HelloTriangleApplication::UpdateRotations()
{
    //<Rotation update
    glm::vec3 EulerAngles = glm::vec3(0, 1, 0.00) * deltaTime; // One revolution per second
    auto MyQuaternion = glm::quat(EulerAngles);

    // Conversion from axis-angle
    // In GLM the angle must be in degrees here, so convert it.

    for (int i = 0; i < scene->objectsCount(); i++)
    {
        scene->objects.rotations[i] *= MyQuaternion;
    }

    scene->Update();
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
void transitionImageForRendering(RendererHandles handles, VkCommandBuffer commandBuffer, semaphoreData waitSemaphores, std::span<VkSemaphore> signalSemaphores, VkImage image, VkImageLayout layoutIn, VkImageLayout layoutOut, VkPipelineStageFlags* waitStages, bool depth)
{

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = 0; // Optional
    beginInfo.pInheritanceInfo = nullptr; // Optional
    //Transition swapchain for rendering
    
    VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));
    TextureUtilities::transitionImageLayout(handles,image,(VkFormat)0,layoutIn,layoutOut,commandBuffer,1, false, depth);
    vkEndCommandBuffer(commandBuffer);

  
    VkSubmitInfo swapChainInSubmitInfo{};
    VkPipelineStageFlags _waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    swapChainInSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    swapChainInSubmitInfo.commandBufferCount = 1;
    swapChainInSubmitInfo.pWaitDstStageMask = waitSemaphores.waitStages.data();
    swapChainInSubmitInfo.pCommandBuffers = &commandBuffer;
    swapChainInSubmitInfo.waitSemaphoreCount = waitSemaphores.semaphores.size();
    swapChainInSubmitInfo.pWaitSemaphores = waitSemaphores.semaphores.data();
    swapChainInSubmitInfo.signalSemaphoreCount = signalSemaphores.size();
    swapChainInSubmitInfo.pSignalSemaphores = signalSemaphores.data();

    vkQueueSubmit(handles.commandPoolmanager->Queues.graphicsQueue, 1, &swapChainInSubmitInfo, VK_NULL_HANDLE);

    ///////////////////////// Transition swapChain  />
    
}
VkPipelineStageFlags swapchainWaitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
VkPipelineStageFlags shadowWaitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
void HelloTriangleApplication::drawFrame()
{
    //Update per frame data

  
    
    updatePerFrameBuffers(currentFrame, scene->objects.matrices); // TODO JS: Figure out
    //Prepare for pass
    uint32_t imageIndex; 
    vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, FramesInFlightData[currentFrame].imageAvailableSemaphores, VK_NULL_HANDLE,
                          &imageIndex);
    ///////////////////////// Transition swapChain  />
    ///
    ///
    ///    //Wait for IMAGE INDEX to be ready to present
    vkWaitForFences(device, 1, &FramesInFlightData[imageIndex].inFlightFences, VK_TRUE, UINT64_MAX);
    MemoryArena::free(&perFrameArenas[imageIndex]); // TODO JS: move --but needs to happen after fence!

    vkResetCommandBuffer(FramesInFlightData[currentFrame].opaqueCommandBuffers, 0);
    vkResetCommandBuffer(FramesInFlightData[currentFrame].shadowCommandBuffers, 0);
    vkResetCommandBuffer(FramesInFlightData[currentFrame].swapchainTransitionOutCommandBuffer, 0);
    vkResetCommandBuffer(FramesInFlightData[currentFrame].swapchainTransitionInCommandBuffer, 0);
    vkResetCommandBuffer(FramesInFlightData[currentFrame].shadowTransitionOutCommandBuffer, 0);
    vkResetCommandBuffer(FramesInFlightData[currentFrame].shadowTransitionInCommandBuffer, 0);
    vkResetFences(device, 1, &FramesInFlightData[currentFrame].inFlightFences);


    ///////////////////////// </Transition swapChain 


    semaphoreData waitSemaphores = getSemaphoreDataFromSemaphores({&FramesInFlightData[currentFrame].imageAvailableSemaphores, 1}, &perFrameArenas[currentFrame]);
    //Transition swapchain for rendering
    transitionImageForRendering(
        getHandles(),
        FramesInFlightData[currentFrame].swapchainTransitionInCommandBuffer,
        waitSemaphores,
        std::span<VkSemaphore>(&FramesInFlightData[currentFrame].swapchaintransitionedInSemaphores, 1),
        swapChainImages[currentFrame],
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, swapchainWaitStages, false);
    ///////////////////////// Transition swapChain  />

    //Transition shadow maps for rendering
    transitionImageForRendering(
    getHandles(),
    FramesInFlightData[currentFrame].shadowTransitionInCommandBuffer,
     {},
   {&FramesInFlightData[imageIndex].shadowtransitionedInSemaphores, 1},
    shadowImages[currentFrame],
    VK_IMAGE_LAYOUT_UNDEFINED,
    VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, shadowWaitStages, true);
    ///////////////////////// Transition swapChain  />
    ///
        //Shadows
        std::vector<VkSemaphore> shadowPassWaitSemaphores = {FramesInFlightData[currentFrame].shadowtransitionedInSemaphores};
        std::vector<VkSemaphore> shadowpasssignalSemaphores = {FramesInFlightData[currentFrame].shadowFinishedSemaphores};
        
        renderShadowPass(currentFrame,currentFrame,  getSemaphoreDataFromSemaphores(shadowPassWaitSemaphores, &perFrameArenas[currentFrame]),shadowpasssignalSemaphores);
        

    // VK_CHECK(vkBeginCommandBuffer(FramesInFlightData[imageIndex].shadowCommandBuffers, &beginInfo));
    // TextureUtilities::transitionImageLayout(getHandles(),shadowImages[currentFrame],shadowFormat,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,FramesInFlightData[imageIndex].shadowCommandBuffers,1, false);
    // vkEndCommandBuffer(FramesInFlightData[imageIndex].shadowCommandBuffers);
    

    //Transition shadow maps for reading
    waitSemaphores = getSemaphoreDataFromSemaphores(shadowpasssignalSemaphores, &perFrameArenas[currentFrame]);
    transitionImageForRendering(
    getHandles(),
    FramesInFlightData[currentFrame].shadowTransitionOutCommandBuffer,
    waitSemaphores,
   {&FramesInFlightData[imageIndex].shadowtransitionedOutSemaphores, 1},
    shadowImages[currentFrame],
    VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, shadowWaitStages, true);

    
    //TODO JS: Enable shadow semaphore
    std::vector<VkSemaphore> opaquePassWaitSemaphores = {FramesInFlightData[currentFrame].shadowtransitionedOutSemaphores, FramesInFlightData[currentFrame].swapchaintransitionedInSemaphores};
    std::vector<VkSemaphore> opaquepasssignalSemaphores = {FramesInFlightData[currentFrame].renderFinishedSemaphores};

    //Opaque
    renderOpaquePass(currentFrame,currentFrame, getSemaphoreDataFromSemaphores(opaquePassWaitSemaphores, &perFrameArenas[currentFrame]),opaquepasssignalSemaphores);



    //Transition swapchain for rendering

    waitSemaphores = getSemaphoreDataFromSemaphores(opaquepasssignalSemaphores, &perFrameArenas[currentFrame]);
    transitionImageForRendering(
    getHandles(),
    FramesInFlightData[currentFrame].swapchainTransitionOutCommandBuffer,
    waitSemaphores,
   {&FramesInFlightData[imageIndex].swapchaintransitionedOutSemaphores, 1},
    swapChainImages[currentFrame],
    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, swapchainWaitStages, false);
    ///////////////////////// Transition swapChain  />

    //Render
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &FramesInFlightData[imageIndex].swapchaintransitionedOutSemaphores;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = {&swapChain};
    presentInfo.pImageIndices = &imageIndex;
    presentInfo.pResults = nullptr; // Optional

    //Present frame
    vkQueuePresentKHR(commandPoolmanager.Queues.presentQueue, &presentInfo);
    

    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void HelloTriangleApplication::renderShadowPass(uint32_t currentFrame, uint32_t imageIndex, semaphoreData waitSemaphores, std::vector<VkSemaphore> signalsemaphores)
{
    recordCommandBufferShadowPass(FramesInFlightData[imageIndex].shadowCommandBuffers, imageIndex);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    VkPipelineStageFlags _waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = waitSemaphores.semaphores.size();
    submitInfo.pWaitSemaphores = waitSemaphores.semaphores.data();
    submitInfo.pWaitDstStageMask = waitSemaphores.waitStages.data();
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &FramesInFlightData[imageIndex].shadowCommandBuffers;

    submitInfo.signalSemaphoreCount = signalsemaphores.size();
    submitInfo.pSignalSemaphores = signalsemaphores.data();
    
    //Submit pass 
    VK_CHECK(vkQueueSubmit(commandPoolmanager.Queues.graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE));
    
    
    
}
void HelloTriangleApplication::renderOpaquePass(uint32_t currentFrame, uint32_t imageIndex, semaphoreData waitSemaphores, std::vector<VkSemaphore> signalsemaphores)
{   
    //Record command buffer for pass
    recordCommandBufferOpaquePass(FramesInFlightData[currentFrame].opaqueCommandBuffers, imageIndex);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    
    VkPipelineStageFlags _waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = waitSemaphores.semaphores.size();
    submitInfo.pWaitSemaphores = waitSemaphores.semaphores.data();
    submitInfo.pWaitDstStageMask = waitSemaphores.waitStages.data();
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &FramesInFlightData[currentFrame].opaqueCommandBuffers;
    
   
    submitInfo.signalSemaphoreCount = signalsemaphores.size();
    submitInfo.pSignalSemaphores = signalsemaphores.data();

    //Submit pass 
    VK_CHECK(vkQueueSubmit(commandPoolmanager.Queues.graphicsQueue, 1, &submitInfo, FramesInFlightData[currentFrame].inFlightFences));
    
}

void HelloTriangleApplication::cleanup()
{
    //TODO clenaup swapchain

    //TODO JS: might be moving somewhere?

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
       VulkanMemory::DestroyBuffer(allocator, FramesInFlightData[i].uniformBuffers.data, FramesInFlightData[i].uniformBuffersMemory);
       VulkanMemory::DestroyBuffer(allocator, FramesInFlightData[i].meshBuffers.data, FramesInFlightData[i].meshBuffersMemory);
       VulkanMemory::DestroyBuffer(allocator, FramesInFlightData[i].lightBuffers.data, FramesInFlightData[i].lightBuffersMemory);
        VulkanMemory::DestroyBuffer(allocator, FramesInFlightData[i].lightBuffers.data, FramesInFlightData[i].shadowDataBuffersMemory);
    }

    vkDestroyDescriptorPool(device, descriptorPool, nullptr);

    descriptorsetLayoutsData.cleanup();


    scene->Cleanup();
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        vkDestroySemaphore(device, FramesInFlightData[i].renderFinishedSemaphores, nullptr);
        vkDestroySemaphore(device, FramesInFlightData[i].imageAvailableSemaphores, nullptr);
        vkDestroySemaphore(device, FramesInFlightData[i].swapchaintransitionedOutSemaphores, nullptr);
        vkDestroySemaphore(device, FramesInFlightData[i].swapchaintransitionedInSemaphores, nullptr);

        //TODO JS: for each 
        vkDestroySemaphore(device, FramesInFlightData[i].shadowtransitionedOutSemaphores, nullptr);
        vkDestroySemaphore(device, FramesInFlightData[i].shadowtransitionedInSemaphores, nullptr);
        

        vkDestroyFence(device, FramesInFlightData[i].inFlightFences, nullptr);
    }

    vkDestroyCommandPool(device, commandPoolmanager.commandPool, nullptr);
    vkDestroyCommandPool(device, commandPoolmanager.transferCommandPool, nullptr);

    for (auto imageView : swapChainImageViews)
    {
        vkDestroyImageView(device, imageView, nullptr);
    }

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        VulkanMemory::DestroyImage(allocator, shadowImages[i], shadowMemory[i]);
        vkDestroySampler(device, shadowSamplers[i], nullptr);
        for(int j = 0; j < MAX_SHADOWMAPS; j++)
        {
            vkDestroyImageView(device, shadowMapRenderingImageViews[i][j], nullptr);
            vkDestroyImageView(device, shadowMapSamplingImageViews[i][j], nullptr);
        }
    }

     // vkb::destroy_surface(vkb_surface);
    destroy_swapchain(vkb_swapchain);
   
    vkDestroySurfaceKHR(instance, surface, nullptr);
    destroy_device(vkb_device);
    destroy_instance(vkb_instance);
    SDL_DestroyWindow(_window);

   
}

#pragma endregion


//TODO move or replace
void SET_UP_SCENE(HelloTriangleApplication* app)
{
    std::vector<int> randomMeshes;
    std::vector<int> randomMaterials;



    int placeholderTextureidx = app->scene->AddMaterial(
        TextureData(app->getHandles(), "textures/pbr_cruiser-panels/space-cruiser-panels2_albedo.png",
                    TextureData::TextureType::DIFFUSE),
        TextureData(app->getHandles(), "textures/pbr_cruiser-panels/space-cruiser-panels2_roughness_metallic.tga",
                    TextureData::TextureType::SPECULAR),
        TextureData(app->getHandles(), "textures/pbr_cruiser-panels/space-cruiser-panels2_normal-dx.png",
                    TextureData::TextureType::NORMAL));
    randomMaterials.push_back(placeholderTextureidx);


    placeholderTextureidx = app->scene->AddMaterial(
        TextureData(app->getHandles(), "textures/pbr_cruiser-panels/space-cruiser-panels2_albedo.png",
                    TextureData::TextureType::DIFFUSE),
        TextureData(app->getHandles(), "textures/pbr_cruiser-panels/space-cruiser-panels2_roughness_metallic.tga",
                    TextureData::TextureType::SPECULAR),
        TextureData(app->getHandles(), "textures/pbr_cruiser-panels/space-cruiser-panels2_normal-dx.png",
                    TextureData::TextureType::NORMAL));
    randomMaterials.push_back(placeholderTextureidx);

    placeholderTextureidx = app->scene->AddMaterial(
        TextureData(app->getHandles(), "textures/pbr_stainless-steel/used-stainless-steel2_albedo.png",
                    TextureData::TextureType::DIFFUSE),
        TextureData(app->getHandles(), "textures/pbr_stainless-steel/used-stainless-steel2_roughness_metallic.tga",
                    TextureData::TextureType::SPECULAR),
        TextureData(app->getHandles(), "textures/pbr_stainless-steel/used-stainless-steel2_normal-dx.png",
                    TextureData::TextureType::NORMAL));
    randomMaterials.push_back(placeholderTextureidx);

    placeholderTextureidx = app->scene->AddMaterial(
        TextureData(app->getHandles(), "textures/pbr_factory-sliding/worn-factory-siding_albedo.png",
                    TextureData::TextureType::DIFFUSE),
        TextureData(app->getHandles(), "textures/pbr_factory-sliding/worn-factory-siding_roughness_metallic.tga",
                    TextureData::TextureType::SPECULAR),
        TextureData(app->getHandles(), "textures/pbr_factory-sliding/worn-factory-siding_normal-dx.png",
                    TextureData::TextureType::NORMAL));
    randomMaterials.push_back(placeholderTextureidx);

    //TODO: Scene loads mesh instead? 
    randomMeshes.push_back(app->scene->AddBackingMesh(MeshData(app->getHandles(), "Meshes/pig.glb")));
    randomMeshes.push_back(app->scene->AddBackingMesh(MeshData(app->getHandles(), "Meshes/cubesphere.glb")));
    randomMeshes.push_back(app->scene->AddBackingMesh(MeshData(app->getHandles(), "Meshes/monkey.obj")));

    int cube = app->scene->AddBackingMesh(MeshData(app->getHandles(), "Meshes/cube.glb"));

    //direciton light
       
    //spot light
    //TODO JS: paramaterize better -- hard to set power and radius currently
    app->scene->AddSpotLight(glm::vec3(2.5, 3, 3.3), glm::vec3(0, 0, -1), glm::vec3(1, 0, 1), 45, 14000);


    //point lights    
    app->scene->AddPointLight(glm::vec3(1, 1, 0), glm::vec3(1, 1, 1), 55);
    app->scene->AddDirLight(glm::vec3(0,0,1), glm::vec3(0,1,0), 3);
    app->scene->AddDirLight(glm::vec3(0.00, 1, 0),  glm::vec3(0, 0, 1), 33);
    app->scene->AddPointLight(glm::vec3(-2, 2, 0), glm::vec3(1, 0, 0), 4422 / 2);
    // app->scene->AddDirLight(glm::vec3(0,0,1), glm::vec3(0,1,0), 3);
    app->scene->AddPointLight(glm::vec3(0, 0, 0), glm::vec3(1, 1, 0), 999 / 2);

    // app->scene->AddPointLight(glm::vec3(0, 8, -10), glm::vec3(0.2, 0, 1), 44 / 2);

  


    glm::vec3 EulerAngles(0, 0, 0);
    auto MyQuaternion = glm::quat(EulerAngles);
    
    
    for (int i = 0; i < 100; i++)
    {
        for (int j = i == 0 ? 1 : 0 ; j < 10; j ++)
        {
            float rowRoughness = glm::clamp(static_cast<float>(i) / 8.0, 0.0, 1.0);
            bool rowmetlalic = i % 3 == 0;
            int textureIndex = rand() % randomMaterials.size();
    
            app->scene->AddObject(
                &app->scene->backing_meshes[randomMeshes[rand() % randomMeshes.size()]],
                randomMaterials[textureIndex], rowRoughness, false,
                glm::vec4((j), (i / 10) * 1.0, - (i % 10), 1) * 1.2f,
                MyQuaternion,
                glm::vec3(0.5));
            textureIndex = rand() % randomMaterials.size();
        }
    
    }

    // add plane
     // app->scene->AddObject( &app->scene->backing_meshes[cube],
     //     0,
     //     0,
     //     false,
     //     glm::vec4(0, 9.35, 0, 1),
     //     glm::quat(),
     //     glm::vec3(20,0.05,30)); // basically a plane

    // add plane
     // app->scene->AddObject( &app->scene->backing_meshes[cube],
     //     0,
     //     0,
     //     false,
     //     glm::vec4(5, 9.35, 5, 1),
     //     glm::quat(),
     //     glm::vec3(0.055,30,10)); // basically a plane
     // app->scene->AddObject( &app->scene->backing_meshes[cube],
     //     0,
     //     0,
     //     false,
     //     glm::vec4(-5, 9.35, 5, 1),
     //     glm::quat(),
     //     glm::vec3(20,30,0.05)); // basically a plane
    // app->scene->AddObject( &app->scene->backing_meshes[cube],
    //   0,
    //   0,
    //   false,
    //   glm::vec4(-5, 9.35, -5, 1),
    //   glm::quat(),
    //   glm::vec3(20,30,0.05)); // basically a plane
    

   
}

