#include "TextureCreationUtilities.h"

#include <atldef.h>
#include <cassert>
#include <cstdio>
#include <cstdlib>

#include <Renderer/VulkanBuffers/bufferCreation.h>
#include <Renderer/CommandPoolManager.h>
#include <Renderer/rendererGlobals.h>
#include <Renderer/VulkanIncludes/Vulkan_Includes.h>
#include "Renderer/RendererContext.h"
#include "Renderer/MainRenderer/VulkanRendererInternals/RendererHelpers.h"


//todo js
struct temporaryTextureInfo
{
    VkImage textureImage;
    VmaAllocation alloc;
    uint64_t width;
    uint64_t height;
    uint64_t mipCt;
};

VkImageView TextureUtilities::createImageViewCustomMip(BufferCreationContext rendererContext, VkImage image,
                                                       VkFormat format,
                                                       VkImageAspectFlags aspectFlags,
                                                       VkImageViewType type, uint32_t layerCount, uint32_t layer,
                                                       uint32_t mipCount, uint32_t baseMip)
{
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = type;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = baseMip;
    viewInfo.subresourceRange.levelCount = mipCount;
    viewInfo.subresourceRange.baseArrayLayer = layer;
    viewInfo.subresourceRange.layerCount = layerCount;

    VkImageView imageView;
    VK_CHECK(vkCreateImageView(rendererContext.device, &viewInfo, nullptr, &imageView));
    setDebugObjectName(rendererContext.device, VK_OBJECT_TYPE_IMAGE_VIEW, "TextureCreation image view",
                       (uint64_t)imageView);
    rendererContext.rendererdeletionqueue->push_backVk(deletionType::ImageView, (uint64_t)imageView);
    return imageView;
}


VkImageView TextureUtilities::createImageView(BufferCreationContext rendererContext, VkImage image, VkFormat format,
                                              VkImageAspectFlags aspectFlags,
                                              VkImageViewType type, uint32_t layerCount, uint32_t layer)
{
    return createImageViewCustomMip(rendererContext, image, format, aspectFlags, type, layerCount, layer,
                                    VK_REMAINING_MIP_LEVELS, 0);
}


void TextureUtilities::createImage(BufferCreationContext rendererContext, uint64_t width, uint64_t height,
                                   VkFormat format,
                                   VkImageTiling tiling,
                                   VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image,
                                   VmaAllocation& allocation, uint32_t miplevels, uint32_t araryLayers,
                                   bool cubeCompatible, const char* debugName)
{
    //TODO JS Properties flags to vma 
    VkImageCreateInfo imageInfo{};

    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = static_cast<uint32_t>(width);
    imageInfo.extent.height = static_cast<uint32_t>(height);
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = miplevels;
    imageInfo.arrayLayers = araryLayers;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (cubeCompatible)
    {
        if (araryLayers > 5 && width == height)
        {
            imageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        }
    }
    BufferUtilities::CreateImage(&imageInfo, &image, &allocation, rendererContext.device, rendererContext.allocator,
                                 debugName);
    rendererContext.rendererdeletionqueue->push_backVMA(deletionType::VmaImage, uint64_t(image), allocation);
}

void TextureUtilities::transitionImageLayout(BufferCreationContext rendererContext, VkImage image, VkFormat format,
                                             VkImageLayout oldLayout,
                                             VkImageLayout newLayout, VkCommandBuffer workingBuffer, uint32_t miplevels,
                                             bool useTransferPool, bool depth)
{
    bool usingTempBuffer = workingBuffer == nullptr;
    CommandBufferPoolQueue tempBufferAndPool{};
    if (usingTempBuffer)
    {
        //Optional buffer for if the caller wants to submit the command to an existing buffer and manually end it later
        tempBufferAndPool = rendererContext.commandPoolmanager->beginSingleTimeCommands(useTransferPool);
        workingBuffer = tempBufferAndPool->buffer;
    }

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;

    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = depth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0; //TODO JS! 
    barrier.subresourceRange.levelCount = miplevels;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

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

    else if ((oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL || oldLayout ==
            VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL) && newLayout ==
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    //Swapchain related 
    else if (oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL && newLayout ==
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
    {
        barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        sourceStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        destinationStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && (newLayout ==
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL || newLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL))
    {
        barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    }
    else
    {
        printf("unsupported layout transition!");
        exit(-1);
    }

    vkCmdPipelineBarrier(
        workingBuffer,
        sourceStage,
        destinationStage,
        0,
        0,
        nullptr,
        0,
        nullptr,
        1,
        &barrier);


    if (usingTempBuffer)
        rendererContext.commandPoolmanager->endSingleTimeCommands(tempBufferAndPool);
}
VkPipelineStageFlags2 deduceStage(VkAccessFlags2 access)
{
    if (access == VK_ACCESS_TRANSFER_WRITE_BIT ||  access ==VK_ACCESS_TRANSFER_READ_BIT)
    {
       return VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    if (access == VK_ACCESS_SHADER_READ_BIT || access == VK_ACCESS_SHADER_WRITE_BIT)
    {
        return VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
    }
    if (access == VK_ACCESS_NONE)
    {
        return 0;
    }
    assert(!"Unhandled stage");
    return 0;
}

VkImageMemoryBarrier2 oneMipBarrier(CommandBufferPoolQueue bandp, VkImage image,
    VkPipelineStageFlags2 srcStage,
    VkPipelineStageFlags2 dstStage,
    VkImageLayout oldLayout,
    VkImageLayout newLayout, uint32_t mipLevel)
{
    VkImageMemoryBarrier2 barrier11 = imageBarrier(image,
                                                VK_ACCESS_MEMORY_WRITE_BIT,
                                                srcStage,
                                                oldLayout,
                                                dstStage,
                                                VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
                                                newLayout,
                                                VK_IMAGE_ASPECT_COLOR_BIT,
                                                mipLevel,
                                                1);
    pipelineBarrier(bandp->buffer, 0, 0, nullptr, 1, &barrier11);
    return barrier11;
}
VkImageMemoryBarrier2 blitBarrier(CommandBufferPoolQueue bandp, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevel)
{
    VkImageMemoryBarrier2 barrier11 = imageBarrier(image,
                                                VK_PIPELINE_STAGE_TRANSFER_BIT,
                                               VK_ACCESS_MEMORY_WRITE_BIT,
                                                oldLayout,
                                                VK_PIPELINE_STAGE_TRANSFER_BIT,
                                                 VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
                                                newLayout,
                                                VK_IMAGE_ASPECT_COLOR_BIT,
                                                mipLevel,
                                                1);
    pipelineBarrier(bandp->buffer, 0, 0, nullptr, 1, &barrier11);
    return barrier11;
}
void TextureUtilities::generateMipmaps(BufferCreationContext rendererContext, VkImage image, VkFormat imageFormat,
                                       int32_t texWidth,
                                       int32_t texHeight, uint32_t mipLevels)
{
    CommandBufferPoolQueue bandp = rendererContext.commandPoolmanager->beginSingleTimeCommands(false);
    std::vector<VkImageMemoryBarrier2> barrierMemory= {};

    auto commandBuffer = bandp->buffer;
    setDebugObjectName(rendererContext.device, VK_OBJECT_TYPE_COMMAND_BUFFER, "mipmap commandbuffer",
                       uint64_t(bandp->buffer));
    VkImageMemoryBarrier barrier{};

    uint32_t currentMipLevel = 0;
    int32_t mipWidth = texWidth;
    int32_t mipHeight = texHeight;
    setDebugObjectName(rendererContext.device, VK_OBJECT_TYPE_IMAGE, "mipmap transition image", (uint64_t)image);
    //Transfer the first mip level into transfer src

    //Set up the first miplevels;
        barrierMemory.push_back( oneMipBarrier(bandp, image,  VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
           VK_IMAGE_LAYOUT_UNDEFINED,
           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, (uint32_t)0));

    for (uint32_t nextMipLevel = 1; nextMipLevel < mipLevels; nextMipLevel++)
    {
        // CommandBufferPoolQueue _bandp = rendererContext.commandPoolmanager->beginSingleTimeCommands(false);
        CommandBufferPoolQueue& _bandp = bandp;
        VkImageBlit blit{};
        blit.srcOffsets[0] = {0, 0, 0};
        blit.srcOffsets[1] = {mipWidth, mipHeight, 1};
        blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.mipLevel = nextMipLevel - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount = 1;
        blit.dstOffsets[0] = {0, 0, 0};
        blit.dstOffsets[1] = {mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1};
        blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.mipLevel = nextMipLevel;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount = 1;

        //transition the mip to dst for the blit 
        // barrierMemory.push_back(blitBarrier(bandp, image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, nextMipLevel));
        vkCmdBlitImage(commandBuffer,
                       image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                       image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                       1, &blit,
                       VK_FILTER_LINEAR);
        //Transition the mip back for reading next iteration
        barrierMemory.push_back(blitBarrier(bandp, image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, nextMipLevel));
       
        if (mipWidth > 1) mipWidth /= 2;
        if (mipHeight > 1) mipHeight /= 2;
        // rendererContext.commandPoolmanager->endSingleTimeCommands(_bandp);
    }
    setDebugObjectName(rendererContext.device, VK_OBJECT_TYPE_IMAGE, "FINISHED mipmap blit image", (uint64_t)image);
    //Transfer the src mip levels to read only

    barrierMemory.push_back( oneMipBarrier(bandp, image,  VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
         VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, (uint32_t)0));
    for(size_t i =1; i < mipLevels; i++)
    {
        barrierMemory.push_back( oneMipBarrier(bandp, image,  VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
           VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, (uint32_t)i));

    }


   

    rendererContext.commandPoolmanager->endSingleTimeCommands(bandp);
    setDebugObjectName(rendererContext.device, VK_OBJECT_TYPE_IMAGE, "FINISHED mipmap transition image", (uint64_t)image);
}

void TextureUtilities::copyBufferToImage(CommandPoolManager* commandPoolManager, VkBuffer buffer, VkImage image,
                                         uint32_t width, uint32_t height,
                                         VkCommandBuffer workingBuffer)
{
    bool endNow = false;
    if (workingBuffer == nullptr)
    {
        //Optional buffer for if the caller wants to submit the command to an existing buffer and manually end it later
        workingBuffer = commandPoolManager->beginSingleTimeCommands_transfer()->buffer;
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
        commandPoolManager->endSingleTimeCommands(workingBuffer, VK_NULL_HANDLE);
}
