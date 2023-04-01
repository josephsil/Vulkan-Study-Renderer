
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

#include "meshData.h"
#include "SceneObjectData.h"
#include "Vertex.h"
#include "stb_image.h"
#include "ShaderLoading.h"
#include "TextureData.h"

#include "VkBootstrap.h"
//zoux vkcheck version
#define VK_CHECK(call) \
do { \
VkResult result_ = call; \
assert(result_ == VK_SUCCESS); \
} while (0)

    VkSurfaceKHR surface;

 
    int main()
    {
        HelloTriangleApplication app;

        try
        {
            app.run();
        }
        catch (const std::exception& e)
        {
            std::cerr << e.what() << std::endl;
            return EXIT_FAILURE;
        }

        return EXIT_SUCCESS;
    }

void HelloTriangleApplication::run()
{
    initWindow();
    initVulkan();
    mainLoop();
    cleanup();
}

HelloTriangleApplication::HelloTriangleApplication()
{
    trivertices = {
            {{-1.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
            {{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
            {{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
            {{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},

            {{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
            {{0.5f, -1.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
            {{0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
            {{-0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}}
    };
    triindices = {
        0, 1, 2, 2, 3, 0,
        4, 5, 6, 6, 7, 4
    };
}



void HelloTriangleApplication::initWindow()
{
    // We initialize SDL and create a window with it. 
    SDL_Init(SDL_INIT_VIDEO);

    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN);

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

//TODO JS: Real meshes should be kept by some kind of scene manager 
MeshData _placeholderMesh;
TextureData _placeholderTexture;

Scene scene;
vkb::Instance vkb_instance;

vkb::InstanceBuilder instance_builder;

vkb::PhysicalDevice vkb_physicalDevice;

vkb::Device vkb_device;

vkb::Swapchain vkb_swapchain;

void HelloTriangleApplication:: initVulkan()
{

    //Get instance
       auto instance_builder_return = instance_builder
    .request_validation_layers()
    .use_default_debug_messenger()
    .require_api_version(1, 2, 0)
    .build();
    if (!instance_builder_return) {
        std::cerr << "Failed to create Vulkan instance. Error: " << instance_builder_return.error().message() << "\n";
    }
   
    vkb_instance = instance_builder_return.value();
    instance = vkb_instance.instance;
    debugMessenger = vkb_instance.debug_messenger;

    //Get surface
    SDL_bool err = SDL_Vulkan_CreateSurface(_window, vkb_instance.instance, &surface);
    if (!err)
    {
        throw std::runtime_error("Failed to create SDL surface"); 
    }

    SDL_Vulkan_GetDrawableSize(_window, &WIDTH, &HEIGHT);

    //Get physical device
    vkb::PhysicalDeviceSelector phys_device_selector(vkb_instance);
    auto physical_device_selector_return = phys_device_selector
            .set_minimum_version(1, 1)
            .set_surface(surface)
            .require_separate_transfer_queue()   //NOTE: Not supporting gpus without dedicated queue 
            .require_separate_compute_queue()
            .add_required_extensions(deviceExtensions)
            .select();
    if (!physical_device_selector_return) {
        throw std::runtime_error("Failed to create Physical Device"); 
    }



    vkb_physicalDevice = physical_device_selector_return.value();

    //Get logical device
    vkb::DeviceBuilder device_builder{vkb_physicalDevice};
    
    auto dev_ret = device_builder.build ();
    
    if (!dev_ret) {
        throw std::runtime_error("Failed to create Virtual Device"); 
    }
    vkb_device = dev_ret.value();
    device = vkb_device.device;
    physicalDevice = vkb_physicalDevice.physical_device;

    //Get push descriptor stuff
    
    // The push descriptor update function is part of an extension so it has to be manually loaded
    vkCmdPushDescriptorSetKHR = (PFN_vkCmdPushDescriptorSetKHR)vkGetDeviceProcAddr(device, "vkCmdPushDescriptorSetKHR");
       

    // Get device push descriptor properties (to display them)
       
    VkPhysicalDeviceProperties2KHR deviceProps2{};
    pushDescriptorProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PUSH_DESCRIPTOR_PROPERTIES_KHR;
    deviceProps2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR;
    deviceProps2.pNext = &pushDescriptorProps;

    vkGetPhysicalDeviceProperties2(physicalDevice, &deviceProps2);



    //Get queues -- the dedicated transfer queue will prevent this from running on many gpus
    auto graphics_ret = vkb_device.get_queue(vkb::QueueType::graphics);
    auto present_ret = vkb_device.get_queue(vkb::QueueType::present);
    auto transfer_ret = vkb_device.get_dedicated_queue(vkb::QueueType::transfer);
    if (!transfer_ret)
    {
        throw std::runtime_error("NO DEDICATED TRANSFER QUEUE");
    }
    
    auto compute_ret = vkb_device.get_queue(vkb::QueueType::compute);
    graphicsQueue  = graphics_ret.value();
    presentQueue  = present_ret.value();
    transferQueue  = transfer_ret.value();
    computeQueue  = compute_ret.value();
    graphicsQueueFamily = vkb_device.get_queue_index(vkb::QueueType::graphics).value();
    presentQueueFamily = vkb_device.get_queue_index(vkb::QueueType::present).value();
    transferQueueFamily = vkb_device.get_dedicated_queue_index(vkb::QueueType::transfer).value();
    computeQueueFamily = vkb_device.get_queue_index(vkb::QueueType::compute).value();

    vkb::SwapchainBuilder swapchain_builder{ vkb_device};
    
    vkb_swapchain = swapchain_builder
    .use_default_format_selection()
    .set_desired_present_mode(VK_PRESENT_MODE_MAILBOX_KHR)
    .set_desired_extent(WIDTH,HEIGHT)
    .build ()
    .value();
    
    swapChain = vkb_swapchain.swapchain;
    // createSwapChain();
    
    swapChainImages = vkb_swapchain.get_images().value();
    swapChainImageViews = vkb_swapchain.get_image_views().value();
    swapChainExtent = vkb_swapchain.extent;
    swapChainImageFormat = vkb_swapchain.image_format;

    //Load shaders 
    shaderLoader = new ShaderLoader(device);
    compileShaders();
    createRenderPass();

    //TODO JS: Sped through these parts belo


    //Command buffer stuff
        
    createTransferCommandPool();
    createGraphicsCommandPool();

    
    createCommandBuffers();
    

    //Descriptor set layouts, which we use with push descrpitor stuff to avoid pools or sets 
    createDescriptorSetLayout();
    
    scene = Scene();

    
    graphicsPipeline_1 = createGraphicsPipeline("triangle", renderPass, nullptr);
    graphicsPipeline_2 = createGraphicsPipeline("triangle_alt", renderPass, nullptr);
    int placeholderTextureidx = scene.AddBackingTexture(TextureData(this, "textures/testTexture.jpg"));

    //TODO: Scene loads mesh instead? 
     int placeholderMeshIdx = scene.AddBackingMesh(MeshData::MeshData(this,"viking_room.obj"));
    placeholderMeshIdx = scene.AddBackingMesh(MeshData::MeshData(this, trivertices, triindices));
    placeholderMeshIdx = scene.AddBackingMesh(MeshData::MeshData(this,"monkey.obj"));
    
    //TODO JS: the mesh memory (backing _placeholdermesh) should probably go to scene too?

  

    glm::vec3 EulerAngles(0, 0, 0);
    auto MyQuaternion = glm::quat(EulerAngles);

    for(int i = 0; i < 1000; i++)
    {
        int random_number = rand() % 3;
        sceneObjects.push_back(
            scene.AddObject(
                 &scene.backing_meshes[random_number],
                &scene.backing_textures[placeholderTextureidx],
                glm::vec4(0,- i * 0.2,0,1),
                MyQuaternion));
        random_number = rand() % 3;
        sceneObjects.push_back(
            scene.AddObject(
            &scene.backing_meshes[random_number],
           &scene.backing_textures[placeholderTextureidx],
                glm::vec4(2,- i * 0.2,0.0,1),
                MyQuaternion));
        random_number = rand() % 3;
        sceneObjects.push_back(
               scene.AddObject(
               &scene.backing_meshes[random_number],
         &scene.backing_textures[placeholderTextureidx],
                   glm::vec4(-2,- i * 0.2,-0.0,1),
                   MyQuaternion));
    }

    // scene.Sort();
    createUniformBuffers();


    createDescriptorSetLayout();
    createSyncObjects();

    createDepthResources();
    createFramebuffers();
}

#pragma region images 
VkImageView HelloTriangleApplication::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags)
{
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VkImageView imageView;
    if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create texture image view!");
    }

    return imageView;
}

void HelloTriangleApplication::transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout,
    VkImageLayout newLayout, VkCommandBuffer workingBuffer)
{
    bool endNow = false;
    if (workingBuffer == nullptr)
    {
        //Optional buffer for if the caller wants to submit the command to an existing buffer and manually end it later
        workingBuffer = beginSingleTimeCommands();
        endNow = true;
    }


    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;

    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0; //TODO JS! 
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    barrier.srcAccessMask = 0; // TODO
    barrier.dstAccessMask = 0; // TODO

    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
    {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout ==
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else
    {
        throw std::invalid_argument("unsupported layout transition!");
    }


    vkCmdPipelineBarrier(
        workingBuffer,
        sourceStage, destinationStage,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );

    if (endNow)
        endSingleTimeCommands(workingBuffer);
}

void HelloTriangleApplication::copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height,
    VkCommandBuffer workingBuffer)
{
    bool endNow = false;
    if (workingBuffer == nullptr)
    {
        //Optional buffer for if the caller wants to submit the command to an existing buffer and manually end it later
        workingBuffer = beginSingleTimeCommands();
        endNow = true;
    }

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;

    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0; //TODO JS
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;

    region.imageOffset = {0, 0, 0};
    region.imageExtent = {
        width,
        height,
        1
    };

    vkCmdCopyBufferToImage(
        workingBuffer,
        buffer,
        image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &region
    );

    if (endNow)
        endSingleTimeCommands(workingBuffer);
}


void HelloTriangleApplication::createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling,
    VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory)
{
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create image!");
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to allocate image memory!");
    }

    vkBindImageMemory(device, image, imageMemory, 0);
}



#pragma endregion

#pragma region descriptor sets

//TODO JS: Move this to scene -- these buffers belong to scene
//TODO JS: not sure how to abstract the frames in flight thing it sucks
//TODO JS: I guess the fn should stay here, and scene should call in, and the fn should get a "packed count" argument for the *3?
void HelloTriangleApplication::createUniformBuffers()
{
    VkDeviceSize bufferSize = sizeof(UniformBufferObject) * scene.matrices.size();

    uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    uniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
    uniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, uniformBuffers[i],
                     uniformBuffersMemory[i]);

        vkMapMemory(device, uniformBuffersMemory[i], 0, bufferSize, 0, &uniformBuffersMapped[i]);
    }
}


//TODO JS: Better understand what needs to be done at startup
    void HelloTriangleApplication::createDescriptorSetLayout()
{
    VkDescriptorSetLayoutBinding pushDescriptorBinding{};
    pushDescriptorBinding.binding = 0; //b0
    pushDescriptorBinding.descriptorCount = 1;
    pushDescriptorBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    pushDescriptorBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushDescriptorBinding.pImmutableSamplers = nullptr; // Optional

    VkDescriptorSetLayoutBinding samplerLayoutBinding{};
    samplerLayoutBinding.binding = 1;
    samplerLayoutBinding.descriptorCount = 1;
    samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBinding.pImmutableSamplers = nullptr;
    samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    
    std::array<VkDescriptorSetLayoutBinding, 2> pushConstantBindings = {pushDescriptorBinding, samplerLayoutBinding};


    VkDescriptorSetLayoutCreateInfo pushDescriptorLayout{};
    pushDescriptorLayout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    //Set flag to use push descriptors
    pushDescriptorLayout.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
    pushDescriptorLayout.bindingCount = static_cast<uint32_t>(pushConstantBindings.size());
    pushDescriptorLayout.pBindings = pushConstantBindings.data();

    VK_CHECK(vkCreateDescriptorSetLayout(device, &pushDescriptorLayout, nullptr, &pushDescriptorSetLayout));
   
}
#pragma endregion 

//TODO JS: ??
#pragma region buffer creation and tools 
void HelloTriangleApplication::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
    VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory)
{
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create buffer!");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to allocate buffer memory!");
    }

    vkBindBufferMemory(device, buffer, bufferMemory, 0);
}

void HelloTriangleApplication::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
{
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    endSingleTimeCommands(commandBuffer);
}



#pragma endregion

#pragma region Begin/End commands
VkCommandBuffer HelloTriangleApplication::beginSingleTimeCommands()
{
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = transferCommandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

void HelloTriangleApplication::endSingleTimeCommands(VkCommandBuffer commandBuffer)
{
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(transferQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(transferQueue);

    vkFreeCommandBuffers(device, transferCommandPool, 1, &commandBuffer);
}

#pragma endregion

#pragma region utility
uint32_t HelloTriangleApplication::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
    {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
        {
            return i;
        }
    }

    throw std::runtime_error("failed to find suitable memory type!");
}

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
//TODO JS: This is like, per object uniforms -- it should belong to the scene and get passed a buffer directly to the render loop
//TODO: Separate the per model xforms from the camera xform
void HelloTriangleApplication::updateUniformBuffers(uint32_t currentImage, std::vector<glm::mat4> models)
{
    std::vector<UniformBufferObject> ubos;

        UniformBufferObject ubo{};

    for(int i = 0; i < models.size(); i++)
    {
        ubo.model = models[i];
        ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));

        ubo.proj = glm::perspective(glm::radians(70.0f), swapChainExtent.width / (float)swapChainExtent.height, 0.1f,
                                    1000.0f);

        ubo.proj[1][1] *= -1;
        ubos.push_back(ubo);
    }

    memcpy(uniformBuffersMapped[currentImage], ubos.data(), sizeof(UniformBufferObject) * models.size()); 
}

void HelloTriangleApplication::updateUniformBuffer(uint32_t currentImage, glm::mat4 model)
{

        UniformBufferObject ubo{};

        ubo.model = model;
        ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));

        ubo.proj = glm::perspective(glm::radians(70.0f), swapChainExtent.width / (float)swapChainExtent.height, 0.1f,
                                    10.0f);

        ubo.proj[1][1] *= -1;
  

    memcpy(uniformBuffersMapped[currentImage], &ubo, sizeof(UniformBufferObject)); 
}

//TODO is this doing extra work?
void HelloTriangleApplication::recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex,
    VkPipeline graphicsPipeline, MeshData* _mesh)
{
    auto mesh = *_mesh;
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
    viewport.width = (float)swapChainExtent.width;
    viewport.height = (float)swapChainExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = swapChainExtent;
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);


    //Bind mesh buffers
    VkDeviceSize offsets[] = {0};

    bool meshbound = false;
    int lastMeshID = -1;
    bool matbound = false;

    //Loop over objects and set push constant stuff
    //TODO JS: to scene object count
    for(int i = 0; i < scene.meshes.size(); i++ )
    {
        

        //TODO JS: Some kind of ID?
       
        bool differentMaterial = false;

        if (!meshbound || scene.meshes[i]->id != lastMeshID)
        {
          
            //TODO JS: loop here too, only apply when mesh changes
            VkBuffer vertBufferAray[]{scene.meshes[i]->vertBuffer};
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertBufferAray, offsets);
            vkCmdBindIndexBuffer(commandBuffer, scene.meshes[i]->indexBuffer, 0, VK_INDEX_TYPE_UINT32);
            lastMeshID = scene.meshes[i]->id;
            meshbound = true;
        }

            // /bind mesh buffers
            VkDescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = uniformBuffers[0]; //TODO: For loop over frames
            bufferInfo.offset = i * sizeof(UniformBufferObject);
            bufferInfo.range = sizeof(UniformBufferObject);
        
            VkDescriptorImageInfo imageInfo{};
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo.imageView = scene.materials[i].texture->textureImageView;
            imageInfo.sampler = scene.materials[i].texture->textureSampler;

            std::array<VkWriteDescriptorSet, 2> writeDescriptorSets{};

            //One big limitation with push descriptor sets is that I can't have more than one
            //So i always update everything per-draw if anything has changed
            // UBO
            writeDescriptorSets[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writeDescriptorSets[0].dstSet = 0;
            writeDescriptorSets[0].dstBinding = 0;
            writeDescriptorSets[0].descriptorCount = 1;
            writeDescriptorSets[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            writeDescriptorSets[0].pBufferInfo = &bufferInfo;


            writeDescriptorSets[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writeDescriptorSets[1].dstBinding = 1;
            writeDescriptorSets[1].dstArrayElement = 0;
            writeDescriptorSets[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writeDescriptorSets[1].descriptorCount = 1;
            writeDescriptorSets[1].pImageInfo = &imageInfo;
            //3 based on my layout

        PerDrawPushConstants constants;
        constants.test = glm::vec4(1,0,0,1);
        vkCmdPushDescriptorSetKHR(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, writeDescriptorSets.size(), writeDescriptorSets.data());

        vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PerDrawPushConstants), &constants);

        vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(scene.meshes[i]->indices.size()), 1, 0, 0, 0);
    }
    
    vkCmdEndRenderPass(commandBuffer);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to record command buffer!");
    }
}
#pragma endregion

#pragma region Command Pools/Queues 
void HelloTriangleApplication::createGraphicsCommandPool()
{
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = graphicsQueueFamily;

   VK_CHECK(vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool));
}

void HelloTriangleApplication::createTransferCommandPool()
{
  
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = transferQueueFamily;

    VK_CHECK(vkCreateCommandPool(device, &poolInfo, nullptr, &transferCommandPool));
   
}

#pragma endregion

void HelloTriangleApplication::createCommandBuffers()
{
    commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = (uint32_t)commandBuffers.size();

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
    VkFormat depthFormat = findDepthFormat();

    createImage(swapChainExtent.width, swapChainExtent.height, depthFormat, VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, depthImage,
                depthImageMemory);
    depthImageView = createImageView(depthImage, depthFormat);
}

VkFormat HelloTriangleApplication::findDepthFormat()
{
    return findSupportedFormat(
        {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
    );
}


VkFormat HelloTriangleApplication::findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling,
    VkFormatFeatureFlags features)
{
    for (VkFormat format : candidates)
    {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);

        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features)
        {
            return format;
        }
        else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features)
        {
            return format;
        }
    }


    throw std::runtime_error("failed to find supported format!");
}

#pragma endregion

void HelloTriangleApplication::createRenderPass()
{
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapChainImageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = findDepthFormat();
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create render pass!");
    }
}

//TODO JS: Reduce graphics pipeline creation
//TODO JS: Understand what we need per object
//TODO JS: Understand when we can generate this
VkPipeline HelloTriangleApplication::createGraphicsPipeline(const char* shaderName, VkRenderPass renderPass,
    VkPipelineCache pipelineCache)
{
    VkPipeline newGraphicsPipeline; //This guy is getting initialized and returned 
    auto shaders = shaderLoader->compiledShaders[shaderName];

    VkPipelineShaderStageCreateInfo shaderStages[] = {shaders[0], shaders[1]};


    auto bindingDescription = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();


    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

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
    pipelineLayoutInfo.setLayoutCount = 2; // Optional
    //TODO JS: These always use the same descriptor set layout currently
    VkDescriptorSetLayout setLayouts[] = {pushDescriptorSetLayout, perMaterialSetLayout};
    pipelineLayoutInfo.pSetLayouts = setLayouts;

    //setup push constants
    VkPushConstantRange push_constant;
    //this push constant range starts at the beginning
    push_constant.offset = 0;
    //this push constant range takes up the size of a MeshPushConstants struct
    push_constant.size = sizeof(PerDrawPushConstants);
    //this push constant range is accessible only in the vertex shader
    push_constant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

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
    while (!bQuit)
    {
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
            }
        }

        //Temp placeholder for like, object loop
        UpdateRotations();
        scene.Update();

        drawFrame();
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

    for (int i = 0; i < scene.rotations.size(); i++)
    {
        scene.rotations[i] *= MyQuaternion; 
    }

    scene.Update();
}

void HelloTriangleApplication::drawFrame()
{
    vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

    uint32_t imageIndex;
    vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE,
                          &imageIndex);

    vkResetCommandBuffer(commandBuffers[currentFrame], 0);


    //TODO: draw multiple objects
    // updateUniformBuffer(currentFrame, scene.matrices[0]);
    updateUniformBuffers(currentFrame, scene.matrices); // TODO JS: Figure out  

    vkResetFences(device, 1, &inFlightFences[currentFrame]);
    //TODO JS: properly manage multiple objects with vertex buffer + corresponding pipeline object
    //VkBuffer vertexBuffers[] = { vertexBuffer };
    
    //TODO: draw multiple objects
    recordCommandBuffer(commandBuffers[currentFrame], imageIndex,
                        _selectedShader == 0 ? graphicsPipeline_1 : graphicsPipeline_2, scene.meshes[1]);

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

    if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS)
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

    vkQueuePresentKHR(presentQueue, &presentInfo);

    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void HelloTriangleApplication::cleanup()
{
    //TODO clenaup swapchain
    placeholderTexture->cleanup();


    //TODO JS: might be moving somewhere?

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        vkDestroyBuffer(device, uniformBuffers[i], nullptr);
        vkFreeMemory(device, uniformBuffersMemory[i], nullptr);
    }

    // vkDestroyDescriptorPool(device, descriptorPool, nullptr);

    vkDestroyDescriptorSetLayout(device, pushDescriptorSetLayout, nullptr);


scene.Cleanup();
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
        vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
        vkDestroyFence(device, inFlightFences[i], nullptr);
    }

    vkDestroyCommandPool(device, commandPool, nullptr);
    vkDestroyCommandPool(device, transferCommandPool, nullptr);


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


    vkb::destroy_swapchain(vkb_swapchain);
    vkDestroySwapchainKHR(device, swapChain, nullptr);

    vkDestroyDevice(device, nullptr);

    vkDestroySurfaceKHR(instance, surface, nullptr);

    vkDestroyInstance(instance, nullptr);

    // vkb::destroy_surface(vkb_surface);
    vkb::destroy_device(vkb_device);
    vkb::destroy_instance(vkb_instance);
    SDL_DestroyWindow(_window);
}

#pragma endregion

//TODO JS: This shouldn't be in the class
std::vector<char> HelloTriangleApplication::readFile(const std::string& filename)
{
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open())
    {
        throw std::runtime_error("failed to open file!");
    }

    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);

    file.close();

    return buffer;
}
