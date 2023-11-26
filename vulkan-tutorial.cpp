
#include "vulkan-tutorial.h"
#include<glm/gtc/quaternion.hpp>
#undef main
#define SDL_MAIN_HANDLED 
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

#include <array>
#include <cstdlib>
#include <fstream>
#include <iostream>


#include "bufferCreation.h"
#include "CommandPoolManager.h"
#include "meshData.h"
#include "SceneObjectData.h"
#include "Vertex.h"
#include "ImageLibraryImplementations.h"
#include "Memory.h"
#include "ShaderLoading.h"
#include "textureCreation.h"
#include "TextureData.h"
#include "VkBootstrap.h"
#include "vulkan-utilities.h"
//zoux vkcheck version


int SHADER_MODE;
VkSurfaceKHR surface;

#pragma region vkb setup
vkb::Instance GET_INSTANCE()
{
    vkb::InstanceBuilder instance_builder;
    auto instanceBuilderResult = instance_builder
                                  .request_validation_layers()
                                 .use_default_debug_messenger()
                                 .require_api_version(1, 3, 0)
                                 .build();
    if (!instanceBuilderResult)
    {
        std::cerr << "Failed to create Vulkan instance. Error: " << instanceBuilderResult.error().message() << "\n";
        exit(1);
    }

    return instanceBuilderResult.value();
}

vkb::PhysicalDevice GET_GPU(vkb::Instance instance)
{

    const std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_MAINTENANCE_4_EXTENSION_NAME, VK_KHR_MAINTENANCE_3_EXTENSION_NAME, VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME, VK_EXT_MUTABLE_DESCRIPTOR_TYPE_EXTENSION_NAME
    };
    VkPhysicalDeviceFeatures features{};
    VkPhysicalDeviceVulkan11Features features11{};
    VkPhysicalDeviceVulkan12Features features12{};
    VkPhysicalDeviceVulkan13Features features13{};
    features12.descriptorIndexing = VK_TRUE;
    features12.runtimeDescriptorArray = VK_TRUE;
    features13.dynamicRendering = VK_TRUE;


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
        std::cerr << ("Failed to create Physical Device");
        exit(1);
    }


    return physicalDeviceBuilderResult.value();
}

vkb::Device GET_DEVICE(vkb::PhysicalDevice gpu)
{
    vkb::DeviceBuilder device_builder{gpu};

    auto devicebuilderResult = device_builder.build();

    if (!devicebuilderResult)
    {
        std::cerr << ("Failed to create Virtual Device");
        exit(1);
    }

    return devicebuilderResult.value();
}

#pragma endregion

int main()
{
    HelloTriangleApplication app;

    return EXIT_SUCCESS;
}


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
    alignas(16) glm::vec4 padding;
    alignas(16) glm::vec4 padding2;
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

//TODO JS: replace phys device, device, etc members with a rendererhandles instance?
RendererHandles HelloTriangleApplication::getHandles()
{
    return RendererHandles(physicalDevice, device, &commandPoolmanager, allocator);
}



void HelloTriangleApplication::initVulkan()
{
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

    uint32_t shadowmapsize = 512;
    shadowFormat = VK_FORMAT_D32_SFLOAT;
    //Create shadow image(s) -- TODO JS: don't do this every frame
    shadowImages.resize(MAX_FRAMES_IN_FLIGHT);
    shadowMemory.resize(MAX_FRAMES_IN_FLIGHT);
    shadowImageViews.resize(MAX_FRAMES_IN_FLIGHT);
    for(int i = 0; i < MAX_FRAMES_IN_FLIGHT ; i++)
    {
        TextureUtilities::createImage(getHandles(),shadowmapsize, shadowmapsize,shadowFormat,VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                       VK_IMAGE_USAGE_SAMPLED_BIT,0,shadowImages[i],shadowMemory[i],1);
        shadowImageViews[i] = TextureUtilities::createImageView(device,shadowImages[i],shadowFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
    }
    
    createDepthResources();

    //Load shaders 
    shaderLoader = new ShaderLoader(device);
    shaderLoader->AddShader("triangle", L"./Shaders/Shader1.hlsl");
    shaderLoader->AddShader("triangle_alt", L"./Shaders/shader2.hlsl");
    shaderLoader->AddShader("shadow", L"./Shaders/bindlessShadow.hlsl");

    //Command buffer stuff
    createCommandBuffers();

    //Initialize scene
    scene = std::make_unique<Scene>(Scene());
    SET_UP_SCENE(this);

    //Initialize scene-ish objects we don't have a place for yet 
    cubemaplut_utilitytexture_index = scene->AddUtilityTexture(
        TextureData(getHandles(), "textures/outputLUT.png", TextureData::LINEAR_DATA));
    cube_irradiance = TextureData(getHandles(), "textures/output_cubemap2_diff8.ktx2", TextureData::TextureType::CUBE);
    cube_specular = TextureData(getHandles(), "textures/output_cubemap2_spec8.ktx2", TextureData::TextureType::CUBE);

    
    descriptorsetLayoutsData = DescriptorSets::bindlessDrawData(getHandles(), scene.get());


    //TODO JS: Make pipelines belong to the perPipelineLayout members
    createGraphicsPipeline("triangle",  &descriptorsetLayoutsData);
    createGraphicsPipeline("triangle_alt",  &descriptorsetLayoutsData);
    //TODO JS: separate shadow layout?
    createGraphicsPipeline("shadow",  &descriptorsetLayoutsData, true);
    
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
    std::vector<gpuvertex> verts;
    for (int j = 0; j < scene->backing_meshes.size(); j++)
    {
        MeshData mesh = scene->backing_meshes[j];
        for (int i = 0; i < mesh.indices.size(); i++)
        {
            glm::vec4 pos = mesh.vertices[mesh.indices[i]].pos;
            glm::vec4 col = mesh.vertices[mesh.indices[i]].color;
            glm::vec4 uv = mesh.vertices[mesh.indices[i]].texCoord;
            glm::vec4 norm = mesh.vertices[mesh.indices[i]].normal;
            glm::vec4 tangent = mesh.vertices[mesh.indices[i]].tangent;
            gpuvertex vert = {
                glm::vec4(pos.x, pos.y, pos.z, 1), uv, norm, glm::vec4(tangent.x, tangent.y, tangent.z, tangent.w)
            };
            verts.push_back(vert);
        }
    }
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        memcpy(FramesInFlightData[i].meshBuffersMapped, verts.data(), sizeof(gpuvertex) * scene->getVertexCount());
    }
}

#pragma region descriptor sets

//TODO JS: Move?
//TODO JS: https://gpuopen-librariesandsdks.github.io/VulkanMemoryAllocator/html/usage_patterns.html advanced
void HelloTriangleApplication::createUniformBuffers()
{
    VkDeviceSize globalsSize = sizeof(ShaderGlobals);
    VkDeviceSize ubosSize = sizeof(UniformBufferObject) * scene->matrices.size();
    VkDeviceSize vertsSize = sizeof(gpuvertex) * scene->getVertexCount();
    VkDeviceSize lightdataSize = sizeof(gpulight) * scene->lightposandradius.size();

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
        FramesInFlightData[i].opaqueShaderGlobalsMapped = BufferUtilities::createDynamicBuffer(
            getHandles(), globalsSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            &FramesInFlightData[i].opaqueShaderGlobalsMemory,
            FramesInFlightData[i].opaqueShaderGlobalsBuffer.data);
        
        FramesInFlightData[i].uniformBuffers.size = ubosSize;
        FramesInFlightData[i].uniformBuffersMapped = BufferUtilities::createDynamicBuffer(
            getHandles(), ubosSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            &FramesInFlightData[i].uniformBuffersMemory,
            FramesInFlightData[i].uniformBuffers.data);
        
        FramesInFlightData[i].meshBuffers.size = vertsSize;
        FramesInFlightData[i].meshBuffersMapped = BufferUtilities::createDynamicBuffer(
            getHandles(), vertsSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            &FramesInFlightData[i].meshBuffersMemory,
            FramesInFlightData[i].meshBuffers.data);

        FramesInFlightData[i].lightBuffers.size = lightdataSize;
        FramesInFlightData[i].lightBuffersMapped = BufferUtilities::createDynamicBuffer(
            getHandles(), lightdataSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            &FramesInFlightData[i].lightBuffersMemory,
            FramesInFlightData[i].lightBuffers.data);

    }
}

void HelloTriangleApplication::createDescriptorSetLayout(VkDescriptorSetLayoutCreateInfo layoutinfo,
                                                         VkDescriptorSetLayout* layout)
{
    VK_CHECK(vkCreateDescriptorSetLayout(device, &layoutinfo, nullptr, layout));
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

void HelloTriangleApplication::updateOpaqueDescriptorSets(RendererHandles handles, VkDescriptorPool pool, DescriptorSets::bindlessDrawData* layoutData)
{

    //Get data
    auto [imageInfos, samplerInfos] = scene->getBindlessTextureInfos();
    auto [cubeImageInfos, cubeSamplerInfos] = DescriptorSets::ImageInfoFromImageDataVec({
        cube_irradiance, cube_specular});
    VkDescriptorBufferInfo meshBufferinfo = FramesInFlightData[currentFrame].meshBuffers.getBufferInfo();
    VkDescriptorBufferInfo lightbufferinfo = FramesInFlightData[currentFrame].lightBuffers.getBufferInfo();
    VkDescriptorBufferInfo uniformbufferinfo = FramesInFlightData[currentFrame].uniformBuffers.getBufferInfo();
    VkDescriptorBufferInfo shaderglobalsinfo = FramesInFlightData[currentFrame].opaqueShaderGlobalsBuffer.getBufferInfo();

    std::vector<descriptorUpdateData> descriptorUpdates;
    //Update descriptor sets with data
 
    descriptorUpdates.push_back({VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &shaderglobalsinfo});
    descriptorUpdates.push_back({VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, imageInfos.data(),  (uint32_t)imageInfos.size()});
    descriptorUpdates.push_back({VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, cubeImageInfos.data(), (uint32_t)cubeImageInfos.size()});
    descriptorUpdates.push_back({VK_DESCRIPTOR_TYPE_SAMPLER, samplerInfos.data(), (uint32_t)samplerInfos.size()});
    descriptorUpdates.push_back({VK_DESCRIPTOR_TYPE_SAMPLER, cubeSamplerInfos.data(), (uint32_t)cubeSamplerInfos.size()});
    descriptorUpdates.push_back({VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &meshBufferinfo});
    descriptorUpdates.push_back({VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &lightbufferinfo});
    descriptorUpdates.push_back({VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &uniformbufferinfo});

    layoutData->updateOpaqueDescriptorSets(descriptorUpdates, currentFrame);
}


//TODO JS: Need to use separate descriptors  
//TODO JS: Probably want to duplicate less code
void HelloTriangleApplication::updateShadowDescriptorSets(RendererHandles handles,  VkDescriptorPool pool, DescriptorSets::bindlessDrawData* layoutData, uint32_t shadowIndex)
{

    //Get data
    auto [imageInfos, samplerInfos] = scene->getBindlessTextureInfos();
    auto [cubeImageInfos, cubeSamplerInfos] = DescriptorSets::ImageInfoFromImageDataVec({
        cube_irradiance, cube_specular});
    VkDescriptorBufferInfo meshBufferinfo = FramesInFlightData[currentFrame].meshBuffers.getBufferInfo();
    VkDescriptorBufferInfo lightbufferinfo = FramesInFlightData[currentFrame].lightBuffers.getBufferInfo();
    VkDescriptorBufferInfo uniformbufferinfo = FramesInFlightData[currentFrame].uniformBuffers.getBufferInfo();
    VkDescriptorBufferInfo shaderglobalsinfo = FramesInFlightData[currentFrame].perLightShadowShaderGlobalsBuffer[shadowIndex].getBufferInfo();

    std::vector<descriptorUpdateData> descriptorUpdates;
    //Update descriptor sets with data
 
    descriptorUpdates.push_back({VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &shaderglobalsinfo});
    descriptorUpdates.push_back({VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, imageInfos.data(),  (uint32_t)imageInfos.size()});
    descriptorUpdates.push_back({VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, cubeImageInfos.data(), (uint32_t)cubeImageInfos.size()});
    descriptorUpdates.push_back({VK_DESCRIPTOR_TYPE_SAMPLER, samplerInfos.data(), (uint32_t)samplerInfos.size()});
    descriptorUpdates.push_back({VK_DESCRIPTOR_TYPE_SAMPLER, cubeSamplerInfos.data(), (uint32_t)cubeSamplerInfos.size()});
    descriptorUpdates.push_back({VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &meshBufferinfo});
    descriptorUpdates.push_back({VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &lightbufferinfo});
    descriptorUpdates.push_back({VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &uniformbufferinfo});

    layoutData->updateShadowDescriptorSets(descriptorUpdates, currentFrame);

    
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
            printf("failed to create synchronization objects for a shadow pass!");
            assert(false);
        }
    }
   

}

#pragma region prepare and submit draw call


void HelloTriangleApplication::updatePerFrameBuffers(uint32_t currentImage, std::vector<glm::mat4> models,
                                                    inputData input)
{
    //Opaque globals
    ShaderGlobals globals;
    eyePos += (input.translate * deltaTime);

    glm::mat4 view = lookAt(eyePos, glm::vec3(0.0f, -1.0f, 0.5f), glm::vec3(0.0f, 0.0f, 1.0f));

    glm::mat4 proj = glm::perspective(glm::radians(70.0f),
                                      swapChainExtent.width / static_cast<float>(swapChainExtent.height), 0.1f,
                                      1000.0f);
    proj[1][1] *= -1;
    globals.view = view;
    globals.proj = proj;
    globals.viewPos = glm::vec4(eyePos.x, eyePos.y, eyePos.z, 1);
    globals.lightcountx_modey_paddingzw = glm::vec4(scene->lightCount, SHADER_MODE, 0, 0);
    globals.cubemaplutidx_cubemaplutsampleridx_paddingzw = glm::vec4(
        scene->materialTextureCount() + cubemaplut_utilitytexture_index,
        scene->materialTextureCount() + cubemaplut_utilitytexture_index, 0, 0);
    memcpy(FramesInFlightData[currentImage].opaqueShaderGlobalsMapped, &globals, sizeof(ShaderGlobals));

    //Shadow globals
    int shadowcount = scene->lightCount > MAX_SHADOWCASTERS ? MAX_SHADOWCASTERS : scene->lightCount;
    for(int i =0; i <shadowcount; i++)
    {
        ShaderGlobals shadowGlobals = {};

        //TODO JS Projection per light
        //TODO JS: Not sure how projection is handled for point lights 
        float near_plane = 1.0f, far_plane = 7.5f;
        glm::mat4 lightProjection = glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, near_plane, far_plane); 
        glm::mat4 lightView = glm::lookAt((glm::vec3)scene->lightposandradius[i],
                                  glm::vec3( 0.0f, 0.0f,  0.0f), 
                                  glm::vec3( 0.0f, 1.0f,  0.0f));
        shadowGlobals.view = lightView;
        shadowGlobals.proj = lightProjection;
        shadowGlobals.viewPos = glm::vec4((glm::vec3)scene->lightposandradius[i],1);

        memcpy(FramesInFlightData[currentImage].perLightShadowShaderGlobalsMapped[i], &shadowGlobals, sizeof(ShaderGlobals));
    }

    //Ubos 
    std::vector<UniformBufferObject> ubos;

    if (ubos.size() != models.size())
    {
        ubos.resize(models.size());
    }
    for (int i = 0; i < models.size(); i++)
    {
        glm::mat4* model = &models[i];
        ubos[i].model = models[i];
        ubos[i].Normal = transpose(inverse(glm::mat3(*model)));
    }
    memcpy(FramesInFlightData[currentImage].uniformBuffersMapped, ubos.data(), sizeof(UniformBufferObject) * models.size());

    //Lights
    std::vector<gpulight> lights;

    lights.resize(scene->lightposandradius.size());

    for (int i = 0; i < scene->lightposandradius.size(); i++)
    {
        lights[i] = {scene->lightposandradius[i], scene->lightcolorAndIntensity[i], glm::vec4(1), glm::vec4(1)};
    }

    memcpy(FramesInFlightData[currentImage].lightBuffersMapped, lights.data(), sizeof(gpulight) * lights.size());

}


void HelloTriangleApplication::recordCommandBufferShadowPass(VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
    uint32_t shadowSize = 512; //TODO JS: make const
     VkCommandBufferBeginInfo beginInfo{};
     beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
     beginInfo.flags = 0; // Optional
     beginInfo.pInheritanceInfo = nullptr; // Optional
     //Transition swapchain for rendering
    
     VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));



     const VkRenderingAttachmentInfoKHR dynamicRenderingDepthAttatchment {
         .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
         .imageView = shadowImageViews[imageIndex],
         .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR,
         .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
         .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
         .clearValue =  {1.0f, 0}
     };

     VkRenderingInfo renderPassInfo{};
     renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
     renderPassInfo.renderArea.extent = {shadowSize , shadowSize }; //TODO JS: Shadow size
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

    uint32_t SHADOW_INDEX = 0; //TODO JS: loop over shadowcasters
     updateShadowDescriptorSets(getHandles(), descriptorPool, &descriptorsetLayoutsData,SHADOW_INDEX);
     descriptorsetLayoutsData.bindToCommandBufferShadow(commandBuffer, currentFrame);
     VkPipelineLayout layout = descriptorsetLayoutsData.getLayoutOpaque();

    //TODO JS: Something other than hardcoded index 2 for shadow pipeline
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, descriptorsetLayoutsData.getPipeline(2));
     int meshct = scene->meshes.size();
     for (int i = 0; i <meshct; i++)
     {
         per_object_data constants;
         //Light count, vert offset, texture index, and object data index
         constants.indexInfo = glm::vec4(-1, (scene->meshOffsets[i]),
                                         -1, i);

         constants.materialprops = glm::vec4(-1, -1, 0, 0);

         vkCmdPushConstants(commandBuffer, layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                            sizeof(per_object_data), &constants);

         vkCmdDraw(commandBuffer, static_cast<uint32_t>(scene->meshes[i]->vertcount), 1, 0, 0);
     }


     vkCmdEndRendering(commandBuffer);

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
    
    int meshct = scene->meshes.size();
    int lastPipelineIndex = -1;
    for (int i = 0; i <meshct; i++)
    {
        Material material = scene->materials[i];
        int pipelineIndex = material.pipelineidx;
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
        constants.indexInfo = glm::vec4(material.backingTextureidx, (scene->meshOffsets[i]),
                                        material.backingTextureidx, i);

        constants.materialprops = glm::vec4(material.roughness, scene->materials[i].metallic, 0, 0);

        vkCmdPushConstants(commandBuffer, layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                           sizeof(per_object_data), &constants);

        vkCmdDraw(commandBuffer, static_cast<uint32_t>(scene->meshes[i]->vertcount), 1, 0, 0);
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


void HelloTriangleApplication::createGraphicsPipeline(const char* shaderName, DescriptorSets::bindlessDrawData* descriptorsetdata, bool shadow)
{
    VkPipeline newGraphicsPipeline; 
    auto shaders = shaderLoader->compiledShaders[shaderName];

    if (!shadow)
    descriptorsetdata->createGraphicsPipeline(shaders,&swapChainColorFormat, &depthFormat, false);
    else
        descriptorsetdata->createGraphicsPipeline(shaders,&swapChainColorFormat, &depthFormat,  true, false, true);
        

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

        auto translate = glm::vec3(0);
        auto mouseRot = glm::vec3(0);
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

                if (e.key.keysym.sym == SDLK_a)
                {
                    translate += glm::vec3(1, 0, 0) * translateSpeed;
                }
                if (e.key.keysym.sym == SDLK_d)
                {
                    translate -= glm::vec3(1, 0, 0) * translateSpeed;
                }
                if (e.key.keysym.sym == SDLK_w)
                {
                    translate += glm::vec3(0, 0, 1) * translateSpeed;
                }
                if (e.key.keysym.sym == SDLK_s)
                {
                    translate -= glm::vec3(0, 0, 1) * translateSpeed;
                }
            }
            else if (e.type == SDL_MOUSEMOTION)
            {
                mouseRot += glm::vec3(0, e.motion.xrel, e.motion.yrel) *= mouseSpeed;
            }
        }

        //Temp placeholder for like, object loop
        UpdateRotations();
        scene->Update();


        inputData input = {translate, mouseRot}; //TODO JS: Translate jerky. polling wrong rate?
        drawFrame(input);
        auto t2 = SDL_GetTicks();
    }
    vkDeviceWaitIdle(device);
}

//Placeholder "gameplay" function
void HelloTriangleApplication::UpdateRotations()
{
    //<Rotation update
    glm::vec3 EulerAngles(0, 0, 0.01);
    auto MyQuaternion = glm::quat(EulerAngles);

    // Conversion from axis-angle
    // In GLM the angle must be in degrees here, so convert it.

    for (int i = 0; i < scene->rotations.size(); i++)
    {
        scene->rotations[i] *= MyQuaternion;
    }

    scene->Update();
}


void HelloTriangleApplication::drawFrame(inputData input)
{

    
    //Update per frame data
    updatePerFrameBuffers(currentFrame, scene->matrices, input); // TODO JS: Figure out
    //Prepare for pass
    uint32_t imageIndex; 
    vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, FramesInFlightData[currentFrame].imageAvailableSemaphores, VK_NULL_HANDLE,
                          &imageIndex);


    vkWaitForFences(device, 1, &FramesInFlightData[imageIndex].inFlightFences, VK_TRUE, UINT64_MAX);
    vkResetCommandBuffer(FramesInFlightData[imageIndex].opaqueCommandBuffers, 0);
    vkResetCommandBuffer(FramesInFlightData[imageIndex].shadowCommandBuffers, 0);
    vkResetCommandBuffer(FramesInFlightData[imageIndex].swapchainTransitionOutCommandBuffer, 0);
    vkResetCommandBuffer(FramesInFlightData[imageIndex].swapchainTransitionInCommandBuffer, 0);
    vkResetFences(device, 1, &FramesInFlightData[imageIndex].inFlightFences);

    ///////////////////////// </Transition swapChain 
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = 0; // Optional
    beginInfo.pInheritanceInfo = nullptr; // Optional
    //Transition swapchain for rendering
    
    VK_CHECK(vkBeginCommandBuffer(FramesInFlightData[imageIndex].swapchainTransitionInCommandBuffer, &beginInfo));
    TextureUtilities::transitionImageLayout(getHandles(),swapChainImages[imageIndex],swapChainColorFormat,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,FramesInFlightData[imageIndex].swapchainTransitionInCommandBuffer,1, false);
    vkEndCommandBuffer(FramesInFlightData[imageIndex].swapchainTransitionInCommandBuffer);

    VkPipelineStageFlags swapchainWaitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    VkSubmitInfo swapChainInSubmitInfo{};
    swapChainInSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    swapChainInSubmitInfo.commandBufferCount = 1;
    swapChainInSubmitInfo.pWaitDstStageMask = swapchainWaitStages;
    swapChainInSubmitInfo.pCommandBuffers = &FramesInFlightData[imageIndex].swapchainTransitionInCommandBuffer;
    swapChainInSubmitInfo.waitSemaphoreCount = 1;
    swapChainInSubmitInfo.pWaitSemaphores = &FramesInFlightData[currentFrame].imageAvailableSemaphores;
    swapChainInSubmitInfo.signalSemaphoreCount = 1;
    swapChainInSubmitInfo.pSignalSemaphores = &FramesInFlightData[currentFrame].swapchaintransitionedInSemaphores;

    vkQueueSubmit(commandPoolmanager.Queues.graphicsQueue, 1, &swapChainInSubmitInfo, VK_NULL_HANDLE);

    ///////////////////////// Transition swapChain  />
        //Shadows
        std::vector<VkSemaphore> shadowPassWaitSemaphores = {FramesInFlightData[currentFrame].swapchaintransitionedInSemaphores};
        std::vector<VkSemaphore> shadowpasssignalSemaphores = {FramesInFlightData[imageIndex].shadowFinishedSemaphores};
        
        renderShadowPass(imageIndex,imageIndex, shadowPassWaitSemaphores,shadowpasssignalSemaphores);


        //TODO JS: Enable shadow semaphore
        std::vector<VkSemaphore> opaquePassWaitSemaphores = {FramesInFlightData[imageIndex].shadowFinishedSemaphores};
        std::vector<VkSemaphore> opaquepasssignalSemaphores = {FramesInFlightData[imageIndex].renderFinishedSemaphores};

    
    //Opaque
    renderOpaquePass(imageIndex,imageIndex, opaquePassWaitSemaphores,opaquepasssignalSemaphores);



    ///////////////////////// </Transition swapChain 
    VkCommandBufferBeginInfo beginInfo2{};
    beginInfo2.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo2.flags = 0; // Optional
    beginInfo2.pInheritanceInfo = nullptr; // Optional
    //Transition swapchain for rendering
    
    VK_CHECK(vkBeginCommandBuffer(FramesInFlightData[imageIndex].swapchainTransitionOutCommandBuffer, &beginInfo2));
    //Transition swapchain for present
    TextureUtilities::transitionImageLayout(getHandles(),swapChainImages[imageIndex],swapChainColorFormat,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,FramesInFlightData[imageIndex].swapchainTransitionOutCommandBuffer,1, false);

    vkEndCommandBuffer(FramesInFlightData[imageIndex].swapchainTransitionOutCommandBuffer);

    VkSubmitInfo swapchainOutsubmitInfo{};
    swapchainOutsubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    swapchainOutsubmitInfo.commandBufferCount = 1;
    swapchainOutsubmitInfo.pWaitDstStageMask = swapchainWaitStages;
    swapchainOutsubmitInfo.pCommandBuffers = &FramesInFlightData[imageIndex].swapchainTransitionOutCommandBuffer;
    swapchainOutsubmitInfo.waitSemaphoreCount = opaquepasssignalSemaphores.size();
    swapchainOutsubmitInfo.pWaitSemaphores =  opaquepasssignalSemaphores.data();
    swapchainOutsubmitInfo.signalSemaphoreCount = 1;
    swapchainOutsubmitInfo.pSignalSemaphores = &FramesInFlightData[currentFrame].swapchaintransitionedOutSemaphores;

    vkQueueSubmit(commandPoolmanager.Queues.graphicsQueue, 1, &swapchainOutsubmitInfo, VK_NULL_HANDLE);
    ///////////////////////// Transition swapChain  />
    
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &FramesInFlightData[currentFrame].swapchaintransitionedOutSemaphores;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = {&swapChain};
    presentInfo.pImageIndices = &imageIndex;
    presentInfo.pResults = nullptr; // Optional

    //Present frame
    vkQueuePresentKHR(commandPoolmanager.Queues.presentQueue, &presentInfo);

    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void HelloTriangleApplication::renderShadowPass(uint32_t currentFrame, uint32_t imageIndex, std::vector<VkSemaphore> waitsemaphores, std::vector<VkSemaphore> signalsemaphores)
{
    recordCommandBufferShadowPass(FramesInFlightData[imageIndex].shadowCommandBuffers, imageIndex);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;


    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = waitsemaphores.size();
    submitInfo.pWaitSemaphores = waitsemaphores.data();
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &FramesInFlightData[imageIndex].shadowCommandBuffers;

    submitInfo.signalSemaphoreCount = signalsemaphores.size();
    submitInfo.pSignalSemaphores = signalsemaphores.data();
    
    //Submit pass 
    VK_CHECK(vkQueueSubmit(commandPoolmanager.Queues.graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE));
    
    
    
}
void HelloTriangleApplication::renderOpaquePass(uint32_t currentFrame, uint32_t imageIndex, std::vector<VkSemaphore> waitsemaphores, std::vector<VkSemaphore> signalsemaphores)
{   
    //Record command buffer for pass
    recordCommandBufferOpaquePass(FramesInFlightData[currentFrame].opaqueCommandBuffers, imageIndex);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = waitsemaphores.size();
    submitInfo.pWaitSemaphores = waitsemaphores.data();
    submitInfo.pWaitDstStageMask = waitStages;
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
    }


    destroy_swapchain(vkb_swapchain);
    vkDestroySwapchainKHR(device, swapChain, nullptr);
    vkDestroyDevice(device, nullptr);
    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyInstance(instance, nullptr);

    // vkb::destroy_surface(vkb_surface);
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

    app->scene->AddLight(glm::vec3(1, 1, 1), glm::vec3(1, 1, 1), 5, 5 / 2);
    app->scene->AddLight(glm::vec3(0, -3, 1), glm::vec3(1, 1, 1), 5, 8 / 2);

    app->scene->AddLight(glm::vec3(0, -16, 1), glm::vec3(0.2, 0, 1), 5, 44 / 2);


    glm::vec3 EulerAngles(0, 0, 0);
    auto MyQuaternion = glm::quat(EulerAngles);

    for (int i = 0; i < 300; i++)
    {
        float rowRoughness = glm::clamp(static_cast<float>(i) / 8.0, 0.0, 1.0);
        bool rowmetlalic = i % 3 == 0;
        int textureIndex = rand() % randomMaterials.size();

        app->scene->AddObject(
            &app->scene->backing_meshes[randomMeshes[1]],
            randomMaterials[textureIndex], rowRoughness, false,
            glm::vec4(0, - i * 0.6, 0, 1),
            MyQuaternion);
        textureIndex = rand() % randomMaterials.size();

        app->scene->AddObject(
            &app->scene->backing_meshes[randomMeshes[0]],
            randomMaterials[textureIndex], rowRoughness, true,
            glm::vec4(2, - i * 0.6, 0.0, 1),
            MyQuaternion);
        textureIndex = rand() % randomMaterials.size();

        app->scene->AddObject(
            &app->scene->backing_meshes[randomMeshes[2]],
            randomMaterials[textureIndex], rowRoughness, false,
            glm::vec4(-2, - i * 0.6, -0.0, 1),
            MyQuaternion);
    }
}

