#define VMA_IMPLEMENTATION

#define STB_IMAGE_IMPLEMENTATION
#include "vulkan-tutorial.h"
#include<glm/gtc/quaternion.hpp>
#undef main
#define SDL_MAIN_HANDLED 
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

#include <array>
#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <fstream>
#include <map>
#include <set>

#include "CommandPoolManager.h"
#include "meshData.h"
#include "SceneObjectData.h"
#include "Vertex.h"
#include "stb_image.h"
#include "ShaderLoading.h"
#include "TextureData.h"
#include "VkBootstrap.h"
#include "tinygltf/tiny_gltf.h"
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
        VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME, VK_KHR_MAINTENANCE_4_EXTENSION_NAME
    };

    vkb::PhysicalDeviceSelector phys_device_selector(instance);
    auto physicalDeviceBuilderResult = phys_device_selector
                                       .set_minimum_version(1, 3)
                                       .set_surface(surface)
                                       .require_separate_transfer_queue()
                                       //NOTE: Not supporting gpus without dedicated queue 
                                       .require_separate_compute_queue()
                                       .set_required_features({
                                           VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES
                                       })
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


void HelloTriangleApplication::initVulkan()
{
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

    //Get push descriptor stuff
    //The push descriptor update function is part of an extension so it has to be manually loaded
    vkCmdPushDescriptorSetKHR = (PFN_vkCmdPushDescriptorSetKHR)vkGetDeviceProcAddr(device, "vkCmdPushDescriptorSetKHR");

    // Get device push descriptor properties (to display them)

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
    swapChainImageFormat = vkb_swapchain.image_format;

    //Load shaders 
    shaderLoader = new ShaderLoader(device);
    compileShaders();
    
    RenderingSetup::createRenderPass(this, {swapChainImageFormat, Capabilities::findDepthFormat(this)}, &renderPass);


    //Command buffer stuff
    createCommandBuffers();

    //Initialize scene
    scene = std::make_unique<Scene>(Scene());
    SET_UP_SCENE(this);

    //Initialize scene-ish objects we don't have a place for yet 
    cubemaplut_utilitytexture_index = scene->AddUtilityTexture(
        TextureData(this, "textures/outputLUT.png", TextureData::LINEAR_DATA));
    cube_irradiance = TextureData(this, "textures/output_cubemap2_diff8.ktx2", TextureData::TextureType::CUBE);
    cube_specular = TextureData(this, "textures/output_cubemap2_spec8.ktx2", TextureData::TextureType::CUBE);

    //Only one dsl right now -- for the bindless ubershader
    DescriptorSetSetup::createBindlessLayout(this, &pushDescriptorSetLayout);

    graphicsPipeline_1 = createGraphicsPipeline("triangle", renderPass, nullptr, pushDescriptorSetLayout);
    graphicsPipeline_2 = createGraphicsPipeline("triangle_alt", renderPass, nullptr, pushDescriptorSetLayout);


    createUniformBuffers();
    createSyncObjects();

    createDepthResources();
    createFramebuffers();

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
        memcpy(meshBuffersMapped[i], verts.data(), sizeof(gpuvertex) * scene->getVertexCount());
    }
}

#pragma region descriptor sets

//TODO JS: Move?
void HelloTriangleApplication::createUniformBuffers()
{
    VkDeviceSize bufferSize = sizeof(ShaderGlobals);

    shaderGlobalsBuffer.resize(MAX_FRAMES_IN_FLIGHT);
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        shaderGlobalsBuffer[i].size = bufferSize;
    }
    shaderGlobalsMemory.resize(MAX_FRAMES_IN_FLIGHT);
    shaderGlobalsMapped.resize(MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        BufferUtilities::createBuffer(this, bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                      shaderGlobalsBuffer[i].data,
                                      shaderGlobalsMemory[i]);

        vkMapMemory(device, shaderGlobalsMemory[i], 0, bufferSize, 0, &shaderGlobalsMapped[i]);
    }


    VkDeviceSize bufferSize1 = sizeof(UniformBufferObject) * scene->matrices.size();

    uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        uniformBuffers[i].size = bufferSize1;
    }
    uniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
    uniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        BufferUtilities::createBuffer(this, bufferSize1, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                      uniformBuffers[i].data,
                                      uniformBuffersMemory[i]);

        vkMapMemory(device, uniformBuffersMemory[i], 0, bufferSize1, 0, &uniformBuffersMapped[i]);
    }

    VkDeviceSize bufferSize2 = sizeof(gpuvertex) * scene->getVertexCount();

    meshBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        meshBuffers[i].size = bufferSize2;
    }
    meshBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
    meshBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        BufferUtilities::createBuffer(this, bufferSize2, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                      meshBuffers[i].data,
                                      meshBuffersMemory[i]);

        vkMapMemory(device, meshBuffersMemory[i], 0, bufferSize2, 0, &meshBuffersMapped[i]);
    }

    VkDeviceSize bufferSize3 = sizeof(gpulight) * scene->lightposandradius.size();

    lightBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        lightBuffers[i].size = bufferSize3;
    }
    lightBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
    lightBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        BufferUtilities::createBuffer(this, bufferSize3, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                      lightBuffers[i].data,
                                      lightBuffersMemory[i]);

        vkMapMemory(device, lightBuffersMemory[i], 0, bufferSize3, 0, &lightBuffersMapped[i]);
    }
}


//TODO JS: Better understand what needs to be done at startup
void HelloTriangleApplication::createDescriptorSetLayout(VkDescriptorSetLayoutCreateInfo layoutinfo,
                                                         VkDescriptorSetLayout* layout)
{
    VK_CHECK(vkCreateDescriptorSetLayout(device, &layoutinfo, nullptr, layout));
}
#pragma endregion

//TODO JS: ??
#pragma region buffer creation and tools


#pragma endregion

#pragma region Begin/End commands


#pragma endregion

#pragma region utility


void HelloTriangleApplication::compileShaders()
{
    shaderLoader = new ShaderLoader(device);

    shaderLoader->AddShader("triangle", L"./Shaders/Shader1.hlsl");
    shaderLoader->AddShader("triangle_alt", L"./Shaders/shader2.hlsl");
}


#pragma endregion

void HelloTriangleApplication::createSyncObjects()
{
    imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create synchronization objects for a frame!");
        }
    }
}

#pragma region prepare and submit draw call

void HelloTriangleApplication::updateLightBuffers(uint32_t currentImage)
{
    std::vector<gpulight> lights;

    lights.resize(scene->lightposandradius.size());

    for (int i = 0; i < scene->lightposandradius.size(); i++)
    {
        lights[i] = {scene->lightposandradius[i], scene->lightcolorAndIntensity[i], glm::vec4(1), glm::vec4(1)};
    }

    memcpy(lightBuffersMapped[currentImage], lights.data(), sizeof(gpulight) * lights.size());
}

//TODO JS: This is like, per object uniforms -- it should belong to the scene and get passed a buffer directly to the render loop
//TODO: Separate the per model xforms from the camera xform

void HelloTriangleApplication::updateUniformBuffers(uint32_t currentImage, std::vector<glm::mat4> models,
                                                    inputData input)
{
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
    memcpy(shaderGlobalsMapped[currentImage], &globals, sizeof(ShaderGlobals));


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
    memcpy(uniformBuffersMapped[currentImage], ubos.data(), sizeof(UniformBufferObject) * models.size());
}

void HelloTriangleApplication::updateUniformBuffer(uint32_t currentImage, glm::mat4 model)
{
    UniformBufferObject ubo{};
    ubo.model = model;
    memcpy(uniformBuffersMapped[currentImage], &ubo, sizeof(UniformBufferObject));
}


//TODO is this doing extra work?
void HelloTriangleApplication::recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex,
                                                   VkPipeline graphicsPipeline, MeshData* _mesh)
{
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = 0; // Optional
    beginInfo.pInheritanceInfo = nullptr; // Optional

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to begin recording command buffer!");
    }

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass;
    renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];

    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = swapChainExtent;

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};

    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);


    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

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

    //*************
    //************
    //** Fill data for Push Descriptor Sets

    auto writeDescriptorSetBuilder = DescriptorDataUtilities::WriteDescriptorSetsBuilder(8);

    VkDescriptorBufferInfo shaderglobalsinfo = shaderGlobalsBuffer[currentFrame].getBufferInfo();
    writeDescriptorSetBuilder.Add(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &shaderglobalsinfo);

    auto [imageInfos, samplerInfos] = scene->getBindlessTextureInfos();
    writeDescriptorSetBuilder.Add(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, imageInfos.data(), imageInfos.size());
    writeDescriptorSetBuilder.Add(VK_DESCRIPTOR_TYPE_SAMPLER, samplerInfos.data(), imageInfos.size());

    VkDescriptorBufferInfo meshBufferinfo = meshBuffers[currentFrame].getBufferInfo();
    writeDescriptorSetBuilder.Add(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &meshBufferinfo);

    VkDescriptorBufferInfo lightbufferinfo = lightBuffers[currentFrame].getBufferInfo();
    writeDescriptorSetBuilder.Add(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &lightbufferinfo);

    VkDescriptorBufferInfo uniformbufferinfo = uniformBuffers[currentFrame].getBufferInfo();
    writeDescriptorSetBuilder.Add(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &uniformbufferinfo);

    auto [cubeImageInfos, cubeSamplerInfos] = DescriptorDataUtilities::ImageInfoFromImageDataVec({
        cube_irradiance, cube_specular
    });
    writeDescriptorSetBuilder.Add(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, cubeImageInfos.data(), cubeImageInfos.size());
    writeDescriptorSetBuilder.Add(VK_DESCRIPTOR_TYPE_SAMPLER, cubeSamplerInfos.data(), cubeSamplerInfos.size());

    vkCmdPushDescriptorSetKHR(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0,
                              writeDescriptorSetBuilder.size(), writeDescriptorSetBuilder.data());

    //Per-Object data, then draw
    for (int i = 0; i < scene->meshes.size(); i++)
    {
        per_object_data constants;
        //Light count, vert offset, texture index, and object data index
        constants.indexInfo = glm::vec4(scene->materials[i].backingTextureidx, (scene->meshOffsets[i]),
                                        scene->materials[i].backingTextureidx, i);

        constants.materialprops = glm::vec4(scene->materials[i].roughness, scene->materials[i].metallic, 0, 0);

        vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT || VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                           sizeof(per_object_data), &constants);

        vkCmdDraw(commandBuffer, static_cast<uint32_t>(scene->meshes[i]->vertcount), 1, 0, 0);
    }

    vkCmdEndRenderPass(commandBuffer);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to record command buffer!");
    }


    frames ++;
    // for(int j = 0; j < frames; j++)
    // {
    //     averageCbTime += pastTimes[j];
    // }
    // averageCbTime /= frames;
}
#pragma endregion

#pragma region Command Pools/Queues


#pragma endregion

void HelloTriangleApplication::createCommandBuffers()
{
    commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPoolmanager.commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

    VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()));
}

void HelloTriangleApplication::createFramebuffers()
{
    swapChainFramebuffers.resize(swapChainImageViews.size());

    for (size_t i = 0; i < swapChainImageViews.size(); i++)
    {
        std::array<VkImageView, 2> attachments = {
            swapChainImageViews[i],
            depthImageView
        };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = swapChainExtent.width;
        framebufferInfo.height = swapChainExtent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create framebuffer!");
        }
    }
}


//TODO JS: What's this for
bool HelloTriangleApplication::hasStencilComponent(VkFormat format)
{
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

#pragma region depth

void HelloTriangleApplication::createDepthResources()
{
    VkFormat depthFormat = Capabilities::findDepthFormat(this);

    TextureUtilities::createImage(this, swapChainExtent.width, swapChainExtent.height, depthFormat,
                                  VK_IMAGE_TILING_OPTIMAL,
                                  VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                  depthImage,
                                  depthImageMemory);
    depthImageView =
        TextureUtilities::createImageView(this->device, depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
}


#pragma endregion


VkPipeline HelloTriangleApplication::createGraphicsPipeline(const char* shaderName, VkRenderPass renderPass,
                                                            VkPipelineCache pipelineCache, VkDescriptorSetLayout layout)
{
    VkPipeline newGraphicsPipeline; //This guy is getting initialized and returned 
    auto shaders = shaderLoader->compiledShaders[shaderName];

    VkPipelineShaderStageCreateInfo shaderStages[] = {shaders[0], shaders[1]};


    auto bindingDescription = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();


    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 0;
    vertexInputInfo.vertexAttributeDescriptionCount = 0;
    vertexInputInfo.pVertexBindingDescriptions = nullptr;
    vertexInputInfo.pVertexAttributeDescriptions = nullptr;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.minSampleShading = 1.0f; // Optional
    multisampling.pSampleMask = nullptr; // Optional
    multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
    multisampling.alphaToOneEnable = VK_FALSE; // Optional

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD; // Optional
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD; // Optional

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.minDepthBounds = 0.0f; // Optional
    depthStencil.maxDepthBounds = 1.0f; // Optional
    depthStencil.stencilTestEnable = VK_FALSE;
    depthStencil.front = {}; // Optional
    depthStencil.back = {}; // Optional

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f; // Optional
    colorBlending.blendConstants[1] = 0.0f; // Optional
    colorBlending.blendConstants[2] = 0.0f; // Optional
    colorBlending.blendConstants[3] = 0.0f; // Optional

    std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();


    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1; // Optional
    //TODO JS: These always use the same descriptor set layout currently
    VkDescriptorSetLayout setLayouts[] = {layout};
    pipelineLayoutInfo.pSetLayouts = setLayouts;

    //setup push constants
    VkPushConstantRange push_constant;
    //this push constant range starts at the beginning
    push_constant.offset = 0;
    //this push constant range takes up the size of a MeshPushConstants struct
    push_constant.size = sizeof(per_object_data);
    //this push constant range is accessible only in the vertex shader
    push_constant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT || VK_SHADER_STAGE_FRAGMENT_BIT;

    pipelineLayoutInfo.pPushConstantRanges = &push_constant;
    pipelineLayoutInfo.pushConstantRangeCount = 1;

    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create pipeline layout!");
    }

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;

    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;

    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;


    pipelineInfo.layout = pipelineLayout;

    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = 0;

    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
    pipelineInfo.basePipelineIndex = -1; // Optional

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &newGraphicsPipeline) !=
        VK_SUCCESS)
    {
        throw std::runtime_error("failed to create graphics pipeline!");
    }


    auto val = shaderLoader->compiledShaders[shaderName];
    //Destroy frag and vert for a given shadername 
    for (auto v : val)
    {
        vkDestroyShaderModule(device, v.module, nullptr);
    }

    return newGraphicsPipeline;
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
    vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

    uint32_t imageIndex;
    vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE,
                          &imageIndex);

    vkResetCommandBuffer(commandBuffers[currentFrame], 0);


    //TODO: draw multiple objects
    // updateUniformBuffer(currentFrame, scene->matrices[0]);
    updateUniformBuffers(currentFrame, scene->matrices, input); // TODO JS: Figure out


    updateLightBuffers(currentFrame);


    vkResetFences(device, 1, &inFlightFences[currentFrame]);
    //TODO JS: properly manage multiple objects with vertex buffer + corresponding pipeline object
    //VkBuffer vertexBuffers[] = { vertexBuffer };

    //TODO: draw multiple objects
    recordCommandBuffer(commandBuffers[currentFrame], imageIndex,
                        _selectedShader == 0 ? graphicsPipeline_1 : graphicsPipeline_2, scene->meshes[1]);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    //TODO JS: understand better 

    VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffers[currentFrame];


    VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[currentFrame]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    auto result = vkQueueSubmit(commandPoolmanager.Queues.graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("failed to submit draw command buffer!");
    }

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = {swapChain};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;

    presentInfo.pResults = nullptr; // Optional

    vkQueuePresentKHR(commandPoolmanager.Queues.presentQueue, &presentInfo);

    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void HelloTriangleApplication::cleanup()
{
    //TODO clenaup swapchain

    //TODO JS: might be moving somewhere?

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        vkDestroyBuffer(device, uniformBuffers[i].data, nullptr);
        vkFreeMemory(device, uniformBuffersMemory[i], nullptr);

        vkFreeMemory(device, meshBuffersMemory[i], nullptr);
        vkFreeMemory(device, lightBuffersMemory[i], nullptr);
    }

    // vkDestroyDescriptorPool(device, descriptorPool, nullptr);

    vkDestroyDescriptorSetLayout(device, pushDescriptorSetLayout, nullptr);


    scene->Cleanup();
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
        vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
        vkDestroyFence(device, inFlightFences[i], nullptr);
    }

    vkDestroyCommandPool(device, commandPoolmanager.commandPool, nullptr);
    vkDestroyCommandPool(device, commandPoolmanager.transferCommandPool, nullptr);


    for (auto framebuffer : swapChainFramebuffers)
    {
        vkDestroyFramebuffer(device, framebuffer, nullptr);
    }

    vkDestroyPipeline(device, graphicsPipeline_1, nullptr);
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);

    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    vkDestroyRenderPass(device, renderPass, nullptr);

    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    for (auto imageView : swapChainImageViews)
    {
        vkDestroyImageView(device, imageView, nullptr);
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
        TextureData(app, "textures/pbr_cruiser-panels/space-cruiser-panels2_albedo.png",
                    TextureData::TextureType::DIFFUSE),
        TextureData(app, "textures/pbr_cruiser-panels/space-cruiser-panels2_roughness_metallic.tga",
                    TextureData::TextureType::SPECULAR),
        TextureData(app, "textures/pbr_cruiser-panels/space-cruiser-panels2_normal-dx.png",
                    TextureData::TextureType::NORMAL));
    randomMaterials.push_back(placeholderTextureidx);


    placeholderTextureidx = app->scene->AddMaterial(
        TextureData(app, "textures/pbr_cruiser-panels/space-cruiser-panels2_albedo.png",
                    TextureData::TextureType::DIFFUSE),
        TextureData(app, "textures/pbr_cruiser-panels/space-cruiser-panels2_roughness_metallic.tga",
                    TextureData::TextureType::SPECULAR),
        TextureData(app, "textures/pbr_cruiser-panels/space-cruiser-panels2_normal-dx.png",
                    TextureData::TextureType::NORMAL));
    randomMaterials.push_back(placeholderTextureidx);

    placeholderTextureidx = app->scene->AddMaterial(
        TextureData(app, "textures/pbr_stainless-steel/used-stainless-steel2_albedo.png",
                    TextureData::TextureType::DIFFUSE),
        TextureData(app, "textures/pbr_stainless-steel/used-stainless-steel2_roughness_metallic.tga",
                    TextureData::TextureType::SPECULAR),
        TextureData(app, "textures/pbr_stainless-steel/used-stainless-steel2_normal-dx.png",
                    TextureData::TextureType::NORMAL));
    randomMaterials.push_back(placeholderTextureidx);

    placeholderTextureidx = app->scene->AddMaterial(
        TextureData(app, "textures/pbr_factory-sliding/worn-factory-siding_albedo.png",
                    TextureData::TextureType::DIFFUSE),
        TextureData(app, "textures/pbr_factory-sliding/worn-factory-siding_roughness_metallic.tga",
                    TextureData::TextureType::SPECULAR),
        TextureData(app, "textures/pbr_factory-sliding/worn-factory-siding_normal-dx.png",
                    TextureData::TextureType::NORMAL));
    randomMaterials.push_back(placeholderTextureidx);

    //TODO: Scene loads mesh instead? 
    randomMeshes.push_back(app->scene->AddBackingMesh(MeshData(app, "pig.glb")));
    randomMeshes.push_back(app->scene->AddBackingMesh(MeshData(app, "cubesphere.glb")));
    randomMeshes.push_back(app->scene->AddBackingMesh(MeshData(app, "monkey.obj")));

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


//TODO JS: This shouldn't be in the class
std::vector<char> HelloTriangleApplication::readFile(const std::string& filename)
{
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open())
    {
        throw std::runtime_error("failed to open file!");
    }

    size_t fileSize = file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);

    file.close();

    return buffer;
}
