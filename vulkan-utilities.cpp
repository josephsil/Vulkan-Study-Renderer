#include "vulkan-utilities.h"

#include <array>
#include <iostream>

#include "AppStruct.h"
#include "CommandPoolManager.h"
#include "TextureData.h"
#include "Vulkan_Includes.h"
#include "SceneObjectData.h"
#include "vmaImplementation.h"


void DestroyBuffer(VkDevice device, VkBuffer buffer)
{
vkDestroyBuffer(device, buffer, nullptr);
}
void FreeMemory(VkDevice device, VkDeviceMemory memory)
{
    vkFreeMemory(device, memory, nullptr);
}

void MapMemory(VkDevice device, VkDeviceMemory memory, VkDeviceSize size, void** data)
{
    vkMapMemory(device, memory, 0, size, 0, data);
}

void UnmapMemory(VkDevice device, VkDeviceMemory memory)
{
    vkUnmapMemory(device, memory);
}

//TODO JS: Connect the two kinds of builders, so we like "fill slots" in the result of this one, and validate type/size?
struct bindingBuilder
{
    int i = 0;
    std::vector<VkDescriptorSetLayoutBinding> data;

    bindingBuilder(int size)
    {
        data.resize(size);
    }
    void addBinding(VkDescriptorType type, VkShaderStageFlags stageflags, uint32_t count = 1)
    {
        VkDescriptorSetLayoutBinding binding{};
        binding.binding = i; //b0
        binding.descriptorCount = count;
        binding.descriptorType = type;
        binding.stageFlags = stageflags;
        data[i] = binding;
        i++;
    }
};
void DescriptorSetSetup::createBindlessLayout(RendererHandles rendererHandles, Scene* scene, VkDescriptorSetLayout* layout)
{
    auto builder = bindingBuilder(8);
    builder.addBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL );
    builder.addBinding(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_SHADER_STAGE_FRAGMENT_BIT,  scene->materialTextureCount()  + scene->utilityTextureCount());
    builder.addBinding(VK_DESCRIPTOR_TYPE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT , scene->materialTextureCount()  + scene->utilityTextureCount());
    builder.addBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
    builder.addBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
    builder.addBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
    builder.addBinding(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_SHADER_STAGE_FRAGMENT_BIT,  2);
    builder.addBinding(VK_DESCRIPTOR_TYPE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT , 2);


    //TODO JS: !!!! Over push descriptor set layout max !!!!!!

    VkDescriptorSetLayoutCreateInfo pushDescriptorLayout{};
    pushDescriptorLayout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    //Set flag to use push descriptors
    pushDescriptorLayout.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR |
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
    pushDescriptorLayout.bindingCount = static_cast<uint32_t>(builder.data.size());
    pushDescriptorLayout.pBindings = builder.data.data();


    VK_CHECK(vkCreateDescriptorSetLayout(rendererHandles.device, &pushDescriptorLayout, nullptr, layout));
}

//TODO JS: VK_KHR_dynamic_rendering gets rid of render pass types and just lets you vkBeginRenderPass
void RenderingSetup::createRenderPass(RendererHandles rendererHandles, RenderTextureFormat passformat, VkRenderPass* pass)
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

    if (vkCreateRenderPass(rendererHandles.device, &renderPassInfo, nullptr, pass) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create render pass!");
    }
}

VkFormat Capabilities::findDepthFormat(RendererHandles rendererHandles)
{
    return findSupportedFormat(rendererHandles,
                               {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
                               VK_IMAGE_TILING_OPTIMAL,
                               VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
    );
}


VkFormat Capabilities::findSupportedFormat(RendererHandles rendererHandles, const std::vector<VkFormat>& candidates,
                                           VkImageTiling tiling,
                                           VkFormatFeatureFlags features)
{
    for (VkFormat format : candidates)
    {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(rendererHandles.physicalDevice, format, &props);

        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features)
        {
            return format;
        }
        if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features)
        {
            return format;
        }
    }

    std::cerr << "failed to find supported format!" << "\n";
    exit(1);
}

uint32_t Capabilities::findMemoryType(RendererHandles rendererHandles, uint32_t typeFilter,
                                      VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(rendererHandles.physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
    {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
        {
            return i;
        }
    }

    throw std::runtime_error("failed to find suitable memory type!");
}

VkImageView TextureUtilities::createImageView(VkDevice device, VkImage image, VkFormat format,
                                              VkImageAspectFlags aspectFlags,
                                              VkImageViewType type, uint32_t miplevels, uint32_t layerCount)
{
    aspectFlags = aspectFlags == -1 ? VK_IMAGE_ASPECT_COLOR_BIT : aspectFlags;
    type = (int)type == -1 ? VK_IMAGE_VIEW_TYPE_2D : type; 
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


void TextureUtilities::createImage(RendererHandles rendererHandles, uint32_t width, uint32_t height, VkFormat format,
                                   VkImageTiling tiling,
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

    if (vkCreateImage(rendererHandles.device, &imageInfo, nullptr, &image) != VK_SUCCESS)
    {
        std::cerr << "failed to create image!" << "\n";
        exit(1);
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(rendererHandles.device, image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = Capabilities::findMemoryType(rendererHandles, memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(rendererHandles.device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS)
    {
        std::cerr << "failed to allocate image memory!" << "\n";
        exit(1);
    }

    vkBindImageMemory(rendererHandles.device, image, imageMemory, 0);
}

void TextureUtilities::transitionImageLayout(RendererHandles rendererHandles, VkImage image, VkFormat format,
                                             VkImageLayout oldLayout,
                                             VkImageLayout newLayout, VkCommandBuffer workingBuffer, uint32_t miplevels)
{
    bool endNow = false;
    if (workingBuffer == nullptr)
    {
        //Optional buffer for if the caller wants to submit the command to an existing buffer and manually end it later
        workingBuffer = rendererHandles.commandPoolmanager->beginSingleTimeCommands_transfer();
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
    barrier.subresourceRange.levelCount = miplevels;
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
        rendererHandles.commandPoolmanager->endSingleTimeCommands(workingBuffer);
}

//TODO JS: Replace with something like https://arm-software.github.io/vulkan-sdk/mipmapping.html
//TODO JS: And return temp buffers so we can use then when writing KTX 

void TextureUtilities::generateMipmaps(RendererHandles rendererHandles, VkImage image, VkFormat imageFormat,
                                       int32_t texWidth,
                                       int32_t texHeight, uint32_t mipLevels)
{
    // Check if image format supports linear blitting
    VkFormatProperties formatProperties;
    vkGetPhysicalDeviceFormatProperties(rendererHandles.physicalDevice, imageFormat, &formatProperties);

    if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))
    {
        throw std::runtime_error("texture image format does not support linear blitting!");
    }

    bufferAndPool bandp = rendererHandles.commandPoolmanager->beginSingleTimeCommands(false);

    auto commandBuffer = bandp.buffer;

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image = image;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.subresourceRange.levelCount = mipLevels;

    int32_t mipWidth = texWidth;
    int32_t mipHeight = texHeight;

    for (uint32_t i = 1; i < mipLevels; i++)
    {
        barrier.subresourceRange.baseMipLevel = i - 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                             0, nullptr,
                             0, nullptr,
                             1, &barrier);

        VkImageBlit blit{};
        blit.srcOffsets[0] = {0, 0, 0};
        blit.srcOffsets[1] = {mipWidth, mipHeight, 1};
        blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.mipLevel = i - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount = 1;
        blit.dstOffsets[0] = {0, 0, 0};
        blit.dstOffsets[1] = {mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1};
        blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.mipLevel = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount = 1;

        vkCmdBlitImage(commandBuffer,
                       image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                       image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                       1, &blit,
                       VK_FILTER_LINEAR);

        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
                             0, nullptr,
                             0, nullptr,
                             1, &barrier);

        if (mipWidth > 1) mipWidth /= 2;
        if (mipHeight > 1) mipHeight /= 2;
    }

    barrier.subresourceRange.baseMipLevel = mipLevels - 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(commandBuffer,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
                         0, nullptr,
                         0, nullptr,
                         1, &barrier);

    rendererHandles.commandPoolmanager->endSingleTimeCommands(bandp);
}

void TextureUtilities::copyBufferToImage(CommandPoolManager* commandPoolManager, VkBuffer buffer, VkImage image,
                                         uint32_t width, uint32_t height,
                                         VkCommandBuffer workingBuffer)
{
    bool endNow = false;
    if (workingBuffer == nullptr)
    {
        //Optional buffer for if the caller wants to submit the command to an existing buffer and manually end it later
        workingBuffer = commandPoolManager->beginSingleTimeCommands_transfer();
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
        commandPoolManager->endSingleTimeCommands(workingBuffer);
}

void BufferUtilities::stageVertexBuffer(RendererHandles rendererHandles, VkDeviceSize bufferSize, VkBuffer& buffer,
                                   VkDeviceMemory& bufferMemory, Vertex* data)
{
    BufferUtilities::stageMeshDataBuffer(rendererHandles,
bufferSize,
buffer,
bufferMemory,
data, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    
}

void BufferUtilities::stageIndexBuffer(RendererHandles rendererHandles, VkDeviceSize bufferSize, VkBuffer& buffer,
                                   VkDeviceMemory& bufferMemory, uint32_t* data)
{
    BufferUtilities::stageMeshDataBuffer(rendererHandles,
bufferSize,
buffer,
bufferMemory,
data, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    
}

void BufferUtilities::stageMeshDataBuffer(RendererHandles rendererHandles, VkDeviceSize bufferSize, VkBuffer& buffer,
                                   VkDeviceMemory& bufferMemory, void* vertices, VkBufferUsageFlags dataTypeFlag)
{
    VkBuffer stagingBuffer;

    VmaAllocation allocation = {};
    
    BufferUtilities::createBuffer(rendererHandles, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                  &allocation, stagingBuffer);

    //Bind to memory buffer
    // TODO JS - Use amd memory allocator
    void* data;
    vmaMapMemory(rendererHandles.allocator, allocation,&data);
    memcpy(data, vertices, bufferSize);
    vmaUnmapMemory(rendererHandles.allocator, allocation);


    BufferUtilities::createBuffer(rendererHandles, bufferSize,
                                  VK_BUFFER_USAGE_TRANSFER_DST_BIT | dataTypeFlag, &allocation, buffer);

    BufferUtilities::copyBuffer(rendererHandles, stagingBuffer, buffer, bufferSize);

    vmaDestroyBuffer(rendererHandles.allocator, stagingBuffer, allocation);
}
void BufferUtilities::createBuffer(RendererHandles rendererHandles, VkDeviceSize size, VkBufferUsageFlags usage,
                                   VmaAllocation* allocation, VkBuffer& buffer)
{
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocator allocator = rendererHandles.allocator;
    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU; //TODO JS: Pass in usage?

    vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &buffer, allocation, nullptr);
}

void BufferUtilities::copyBuffer(RendererHandles rendererHandles, VkBuffer srcBuffer, VkBuffer dstBuffer,
                                 VkDeviceSize size)
{
    VkCommandBuffer commandBuffer = rendererHandles.commandPoolmanager->beginSingleTimeCommands_transfer();

    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    rendererHandles.commandPoolmanager->endSingleTimeCommands(commandBuffer);
}


std::pair<std::vector<VkDescriptorImageInfo>, std::vector<VkDescriptorImageInfo>>
DescriptorDataUtilities::ImageInfoFromImageDataVec(std::vector<TextureData> textures)
{
    std::vector<VkDescriptorImageInfo> imageinfos(textures.size());
    std::vector<VkDescriptorImageInfo> samplerinfos(textures.size());
    for (int i = 0; i < textures.size(); i++)
    {
        imageinfos[i] = VkDescriptorImageInfo{
            .imageView = textures[i].textureImageView, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };
        samplerinfos[i] = VkDescriptorImageInfo{
            .sampler = textures[i].textureSampler, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };
    }

    return std::make_pair(imageinfos, samplerinfos);
}

//TODO JS: Move to textureData? Just return a 

std::pair<VkDescriptorImageInfo, VkDescriptorImageInfo> DescriptorDataUtilities::ImageInfoFromImageData(
    TextureData texture)
{
    return std::make_pair(
        VkDescriptorImageInfo{
            .imageView = texture.textureImageView, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        },
        VkDescriptorImageInfo{
            .sampler = texture.textureSampler, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        }
    );
}

DescriptorDataUtilities::WriteDescriptorSetsBuilder::WriteDescriptorSetsBuilder(int length)
{
    descriptorsets.resize(length);
}

VkWriteDescriptorSet* DescriptorDataUtilities::WriteDescriptorSetsBuilder::data()
{
    return descriptorsets.data();
}

uint32_t DescriptorDataUtilities::WriteDescriptorSetsBuilder::size()
{
    return descriptorsets.size();
}

void DescriptorDataUtilities::WriteDescriptorSetsBuilder::Add(VkDescriptorType type, void* ptr, int count )
{
      {
          descriptorsets[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
          descriptorsets[i].dstBinding = i;
          descriptorsets[i].descriptorCount = count;
          descriptorsets[i].descriptorType = type;
          if (type == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE || type == VK_DESCRIPTOR_TYPE_SAMPLER)
          {
              descriptorsets[i].pImageInfo = static_cast<VkDescriptorImageInfo*>(ptr);
          }
          else
          {
              descriptorsets[i].pBufferInfo = static_cast<VkDescriptorBufferInfo*>(ptr);
          }
          i++;
      }}

void DescriptorDataUtilities::WriteDescriptorSetsBuilder::AddStorageBuffer(dataBuffer storageBuffer)
{
    auto bufferInfo = storageBuffer.getBufferInfo();

    Add(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferInfo);
}

VkDescriptorBufferInfo dataBuffer::getBufferInfo()
{
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = data; //TODO: For loop over frames in flight
    bufferInfo.offset = 0;
    bufferInfo.range = size;
    return bufferInfo;
}
