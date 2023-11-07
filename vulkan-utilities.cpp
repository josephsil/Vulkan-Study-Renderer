#include "vulkan-utilities.h"

#include <array>
#include <iostream>

#include "Vulkan_Includes.h"

#include "vulkan-tutorial.h"
#include "SceneObjectData.h"

void DescriptorSetSetup::createBindlessLayout(HelloTriangleApplication* app, VkDescriptorSetLayout* layout)
{
    VkDescriptorSetLayoutBinding globalsBinding{};
    globalsBinding.binding = 0; //b0
    globalsBinding.descriptorCount = 1;
    globalsBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    globalsBinding.stageFlags = VK_SHADER_STAGE_ALL;
    globalsBinding.pImmutableSamplers = nullptr; // Optional

    VkDescriptorSetLayoutBinding textureLayoutBinding{};
    textureLayoutBinding.binding = 1;
    textureLayoutBinding.descriptorCount = (app->scene->materialTextureCount()) + app->scene->utilityTextureCount();
    textureLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    textureLayoutBinding.pImmutableSamplers = nullptr;
    textureLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding samplerLayoutBinding{};
    samplerLayoutBinding.binding = 2;
    samplerLayoutBinding.descriptorCount = app->scene->materialTextureCount() + app->scene->utilityTextureCount();
    samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    samplerLayoutBinding.pImmutableSamplers = nullptr;
    samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding meshLayoutBinding{};
    meshLayoutBinding.binding = 3;
    meshLayoutBinding.descriptorCount = 1;
    meshLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    meshLayoutBinding.pImmutableSamplers = nullptr;
    meshLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutBinding lightLayoutBinding{};
    lightLayoutBinding.binding = 4;
    lightLayoutBinding.descriptorCount = 1;
    lightLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    lightLayoutBinding.pImmutableSamplers = nullptr;
    lightLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding uboBinding{};
    uboBinding.binding = 5; //b0
    uboBinding.descriptorCount = 1;
    uboBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    uboBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    uboBinding.pImmutableSamplers = nullptr; // Optional

    VkDescriptorSetLayoutBinding cubeTextureBinding{};
    cubeTextureBinding.binding = 6;
    cubeTextureBinding.descriptorCount = 2;
    cubeTextureBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    cubeTextureBinding.pImmutableSamplers = nullptr;
    cubeTextureBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding cubeSamplerBinding{};
    cubeSamplerBinding.binding = 7;
    cubeSamplerBinding.descriptorCount = 2;
    cubeSamplerBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    cubeSamplerBinding.pImmutableSamplers = nullptr;
    cubeSamplerBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;


    std::array<VkDescriptorSetLayoutBinding, 8> pushConstantBindings =
    {
        globalsBinding,
        textureLayoutBinding,
        samplerLayoutBinding,
        meshLayoutBinding,
        lightLayoutBinding,
        uboBinding,
        cubeTextureBinding,
        cubeSamplerBinding
    };

    //TODO JS: !!!! Over push descriptor set layout max !!!!!!

    VkDescriptorSetLayoutCreateInfo pushDescriptorLayout{};
    pushDescriptorLayout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    //Set flag to use push descriptors
    pushDescriptorLayout.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR |
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
    pushDescriptorLayout.bindingCount = static_cast<uint32_t>(pushConstantBindings.size());
    pushDescriptorLayout.pBindings = pushConstantBindings.data();

  
    VK_CHECK(vkCreateDescriptorSetLayout(app->device, &pushDescriptorLayout, nullptr, layout));
}

//TODO JS: VK_KHR_dynamic_rendering gets rid of render pass types and just lets you vkBeginRenderPass
void RenderingSetup::createRenderPass(HelloTriangleApplication* app, RenderTextureFormat passformat, VkRenderPass* pass)
{
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = passformat.ColorFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = passformat.DepthFormat;
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

    if (vkCreateRenderPass(app->device, &renderPassInfo, nullptr, pass) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create render pass!");
    }
}

VkFormat Capabilities::findDepthFormat(HelloTriangleApplication* app)
{
    return Capabilities::findSupportedFormat(app,
        {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
    );
}


VkFormat Capabilities::findSupportedFormat(HelloTriangleApplication* app, const std::vector<VkFormat>& candidates, VkImageTiling tiling,
                                                       VkFormatFeatureFlags features)
{
    for (VkFormat format : candidates)
    {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(app->physicalDevice, format, &props);

        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features)
        {
            return format;
        }
        else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features)
        {
            return format;
        }
    }

    std::cerr << "failed to find supported format!" << "\n";
    exit(1);
}

uint32_t Capabilities::findMemoryType(HelloTriangleApplication* app, uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(app->physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
    {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
        {
            return i;
        }
    }

    throw std::runtime_error("failed to find suitable memory type!");
}

VkImageView TextureUtilities::createImageView(VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags,
                                                      VkImageViewType type, uint32_t miplevels, uint32_t layerCount)
{
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = type;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = 0; //TODO JS: pass in something more robust?
    viewInfo.subresourceRange.levelCount = miplevels;
    viewInfo.subresourceRange.baseArrayLayer = 0; //TODO JS: pass in something more robust?
    viewInfo.subresourceRange.layerCount = layerCount;

    VkImageView imageView;
    if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create texture image view!");
    }

    return imageView;
}


void TextureUtilities::createImage(HelloTriangleApplication* app, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling,
                                           VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image,
                                           VkDeviceMemory& imageMemory, uint32_t miplevels)
{
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = miplevels;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(app->device, &imageInfo, nullptr, &image) != VK_SUCCESS)
    {
        std::cerr << "failed to create image!" << "\n";
    exit(1);
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(app->device, image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = Capabilities::findMemoryType(app, memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(app->device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS)
    {
        std::cerr << "failed to allocate image memory!" << "\n";
    exit(1);
    }

    vkBindImageMemory(app->device, image, imageMemory, 0);
}


