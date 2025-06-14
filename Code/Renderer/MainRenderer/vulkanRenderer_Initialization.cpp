#include "VulkanRenderer.h"

#undef main
#define SDL_MAIN_HANDLED 
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

#include "Renderer/VulkanIncludes/VulkanMemory.h"
#include "VulkanRendererInternals/RendererHelpers.h"

//Split off from VulkanRenderer.cpp, code related to vulkan initialization
void VulkanRenderer::initializeWindow()
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


vkb::PhysicalDevice getPhysicalDevice(vkb::Instance instance, VkSurfaceKHR surface)
{
//#define RENDERDOC_COMPATIBILITY
    const std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_MAINTENANCE_4_EXTENSION_NAME, VK_KHR_MAINTENANCE_3_EXTENSION_NAME,
        VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME, VK_EXT_MUTABLE_DESCRIPTOR_TYPE_EXTENSION_NAME
#ifndef RENDERDOC_COMPATIBILITY
        ,VK_EXT_HOST_IMAGE_COPY_EXTENSION_NAME
#endif
        //for image copy:
        // VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME, VK_KHR_COPY_COMMANDS_2_EXTENSION_NAME, VK_KHR_FORMAT_FEATURE_FLAGS_2_EXTENSION_NAME,
    }; // 
VkPhysicalDeviceFeatures features{};
VkPhysicalDeviceVulkan11Features features11{};
VkPhysicalDeviceVulkan12Features features12{};
VkPhysicalDeviceVulkan13Features features13{};
features.multiDrawIndirect = VK_TRUE;
features.wideLines = VK_TRUE;
features11.shaderDrawParameters = VK_TRUE;
features12.descriptorIndexing = VK_TRUE;
features12.runtimeDescriptorArray = VK_TRUE;
features12.descriptorBindingPartiallyBound = VK_TRUE;
features13.dynamicRendering = VK_TRUE;
features13.synchronization2 = VK_TRUE;
features12.samplerFilterMinmax = VK_TRUE;

#ifndef RENDERDOC_COMPATIBILITY
VkPhysicalDeviceHostImageCopyFeaturesEXT deviceHostExt{};
deviceHostExt =
{
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_IMAGE_COPY_FEATURES_EXT,
    .hostImageCopy = VK_TRUE
};

#endif
vkb::PhysicalDeviceSelector phys_device_selector(instance);
auto physicalDeviceBuilderResult = phys_device_selector
                                   .set_minimum_version(1, 3)
                                   .set_surface(surface)
                                   .require_separate_transfer_queue()
#ifndef RENDERDOC_COMPATIBILITY
                                   .add_required_extension_features(deviceHostExt)
#endif
                                   .set_required_features(features)
                                   .set_required_features_11(features11)
                                   .set_required_features_12(features12)
                                   // .set_required_features({.})
                                   .set_required_features_13(features13)
                                   //NOTE: Not supporting gpus without dedicated queue 
                                   .require_separate_compute_queue()
                                   .add_required_extensions(deviceExtensions).select();


    if (!physicalDeviceBuilderResult)
    {
        printf("Failed to create Physical Device %s \n", physicalDeviceBuilderResult.error().message().data());
        exit(1);
    }

    auto a = physicalDeviceBuilderResult.value().get_extensions();
    for (auto& v : a)
    {
        printf("%s \n", v.c_str());
    }
#ifndef RENDERDOC_COMPATIBILITY
    physicalDeviceBuilderResult.value().enable_extension_if_present(VK_EXT_HOST_IMAGE_COPY_EXTENSION_NAME);
#endif
    return physicalDeviceBuilderResult.value();
}

vkb::Device getDevice(vkb::PhysicalDevice gpu)
{
    vkb::DeviceBuilder device_builder{gpu};

    auto devicebuilderResult = device_builder.build();

    if (!devicebuilderResult)
    {
        printf("Failed to create Virtual Device \n");
        exit(1);
    }

    return devicebuilderResult.value();
}

vkb::Instance getInstance()
{
    vkb::InstanceBuilder instance_builder;
    auto instanceBuilderResult = instance_builder
                                 // .request_validation_layers()
                                 .use_default_debug_messenger()
                                 .enable_extension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME)
                                 .require_api_version(1, 3, 255)
                                 .build();
    if (!instanceBuilderResult)
    {
        printf("Failed to create Vulkan instance. Error: %s, \n", instanceBuilderResult.error().message().data());
        assert(false);
    }

    return instanceBuilderResult.value();
}


GlobalRendererResources static_initializeResources(rendererObjects initializedrenderer,
                                                   MemoryArena::Allocator* allocationArena,
                                                   RendererDeletionQueue* deletionQueue,
                                                   CommandPoolManager* commandPoolmanager)
{
    std::vector<VkImage> swapchainImages = initializedrenderer.swapchain.get_images().value();
    std::vector<VkImageView> swapchainImageViews = initializedrenderer.swapchain.get_image_views().value();
    for (int i = 0; i < swapchainImageViews.size(); i++)
    {
        SetDebugObjectNameS(initializedrenderer.vkbdevice.device, VK_OBJECT_TYPE_IMAGE, "Swapchain image",
                           uint64_t(swapchainImages[i]));
        SetDebugObjectNameS(initializedrenderer.vkbdevice.device, VK_OBJECT_TYPE_IMAGE_VIEW, "Swapchain image view",
                           uint64_t(swapchainImageViews[i]));
    }

    for (int i = 0; i < swapchainImageViews.size(); i++)
    {
        deletionQueue->push_backVk(deletionType::ImageView, uint64_t(swapchainImageViews[i]));
    }

    //Load shaders
    auto shaderLoader = std::make_unique<ShaderLoader>(initializedrenderer.vkbdevice.device);
    shaderLoader->AddShader("triangle", L"./Shaders/Shader1_copy.hlsl");
    shaderLoader->AddShader("triangle_alt", L"./Shaders/shader1.hlsl");
    shaderLoader->AddShader("shadow", L"./Shaders/bindlessShadow.hlsl");
    shaderLoader->AddShader("lines", L"./Shaders/lines.hlsl");
    shaderLoader->AddShader("cull", L"./Shaders/cull_compute.hlsl", true);
    shaderLoader->AddShader("pre_cull_copy", L"./Shaders/drawindirect_copy_compute.hlsl", true);
    shaderLoader->AddShader("mipChain", L"./Shaders/mipchain_compute.hlsl", true);
    shaderLoader->AddShader("debug", L"./Shaders/Shader_Debug_Raymarch.hlsl", false);

    std::vector<DepthBufferInfo> depthBufferPerFrame;
    std::vector<DepthPyramidInfo> depthPyramidPerFrame;
    for(int i =0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        depthBufferPerFrame.push_back(CreateDepthResources(initializedrenderer, allocationArena, deletionQueue,
                                                       commandPoolmanager));
        depthPyramidPerFrame.push_back(CreateDepthPyramidResources(initializedrenderer,  allocationArena,"DepthPyramid", deletionQueue,
                                                               commandPoolmanager));
    }
    return {
        .shaderLoader = std::move(shaderLoader),
        .swapchainImages = swapchainImages,
        .swapchainImageViews = swapchainImageViews,
        .depthBufferInfoPerFrame = depthBufferPerFrame,
        .depthPyramidInfoPerFrame = depthPyramidPerFrame,
    };
}




rendererObjects initializeVulkanObjects(SDL_Window* _window, int WIDTH, int HEIGHT)
{
    //Get instance
    auto vkb_instance = getInstance();

    VkSurfaceKHR surface = {};
    //Get surface
    SDL_bool err = SDL_Vulkan_CreateSurface(_window, vkb_instance.instance, &surface);
    assert(err);

    SDL_Vulkan_GetDrawableSize(_window, &WIDTH, &HEIGHT);

    //Get physical device
    auto vkb_physicalDevice = getPhysicalDevice(vkb_instance, surface);

    //Get logical device
    auto vkb_device = getDevice(vkb_physicalDevice);

    // vkCopyImageToMemoryEXT = (PFN_vkCopyImageToMemoryEXT)VkGetCopy(device, "vkCopyImageToMemoryEXT");
    auto allocator = VulkanMemory::GetAllocator(vkb_device.device, vkb_physicalDevice.physical_device,
                                                vkb_instance.instance);
    registerSuperluminal();

    registerDebugUtilsFn(
        (PFN_vkSetDebugUtilsObjectNameEXT)vkGetDeviceProcAddr(vkb_device.device, "vkSetDebugUtilsObjectNameEXT"));
    
    registerTransitionImagefn(
    (PFN_vkTransitionImageLayoutEXT)vkGetDeviceProcAddr(vkb_device.device, "vkTransitionImageLayoutEXT"));

    registerCopyImageToMemoryFn(
    (PFN_vkCopyImageToMemoryEXT)    vkGetDeviceProcAddr(
        vkb_device.device, "vkCopyImageToMemoryEXT"));

    //Swapchain
    vkb::SwapchainBuilder swapchain_builder{vkb_device};

   auto sawpchainfmt = VkSurfaceFormatKHR
    {
        .format = VK_FORMAT_B8G8R8A8_SRGB,
        .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
    };
    auto vkb_swapchain = swapchain_builder
                         .set_desired_format(sawpchainfmt)
                         .set_desired_present_mode(VK_PRESENT_MODE_MAILBOX_KHR)
                         .set_desired_extent(WIDTH, HEIGHT)
                         .build()
                         .value();


    return {
        .vkbInstance = vkb_instance,
        .vkbPhysicalDevice = vkb_physicalDevice,
        .vkbdevice = vkb_device,
        .vmaAllocator = allocator,
        .surface = surface, //not sure I need surface for anything except cleanup?
        .swapchain = vkb_swapchain,
    };
}
