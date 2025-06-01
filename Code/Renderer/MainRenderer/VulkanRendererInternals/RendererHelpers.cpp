#include "RendererHelpers.h"

#include <Renderer/VulkanBuffers/bufferCreation.h>
#include <Renderer/PerThreadRenderContext.h>
#include <Renderer/TextureCreation/Internal/TextureCreationUtilities.h>
#include <Renderer/vulkan-utilities.h>


#pragma region depth

//zeux code
uint32_t previousPow2(uint32_t v)
{
    uint32_t r = 1;

    while (r * 2 < v)
        r *= 2;

    return r;
}


DepthBufferInfo static_createDepthResources(rendererObjects initializedrenderer,
                                            MemoryArena::memoryArena* allocationArena,
                                            RendererDeletionQueue* deletionQueue,
                                            CommandPoolManager* commandPoolmanager)
{
    auto depthFormat = Capabilities::findDepthFormat(initializedrenderer.vkbPhysicalDevice.physical_device);
    //The two null context will get overwritten by the create  calls below
    DepthBufferInfo bufferInfo = {depthFormat, VK_NULL_HANDLE, VK_NULL_HANDLE, VmaAllocation{}};


    BufferCreationContext context = {
        .device = initializedrenderer.vkbdevice.device,
        .allocator = initializedrenderer.vmaAllocator,
        .rendererdeletionqueue = deletionQueue,
        .commandPoolmanager = commandPoolmanager
    };


    TextureUtilities::createImage(context, initializedrenderer.swapchain.extent.width,
                                  initializedrenderer.swapchain.extent.height, bufferInfo.format,
                                  VK_IMAGE_TILING_OPTIMAL,
                                  VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                  (bufferInfo.image),
                                  (bufferInfo.vmaAllocation), 1, 1, false, "DEPTH TEXTURE");


    bufferInfo.view = TextureUtilities::createImageViewCustomMip(context, (bufferInfo.image), bufferInfo.format,
                                                                 VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_VIEW_TYPE_2D, 1, 0,
                                                                 1, 0);
    SetDebugObjectName(initializedrenderer.vkbdevice.device, VK_OBJECT_TYPE_IMAGE, "DepthBufferInfo image", (uint64_t)bufferInfo.image);
    SetDebugObjectName(initializedrenderer.vkbdevice.device, VK_OBJECT_TYPE_IMAGE_VIEW, "DepthBufferInfo imageview", (uint64_t)bufferInfo.view);
    return bufferInfo;
}

DepthPyramidInfo static_createDepthPyramidResources(rendererObjects initializedrenderer,
                                                    MemoryArena::memoryArena* allocationArena,
                                                    RendererDeletionQueue* deletionQueue,
                                                    CommandPoolManager* commandPoolmanager)
{
    auto depthFormat = Capabilities::findSupportedFormat(initializedrenderer.vkbPhysicalDevice.physical_device,
                                                         {VK_FORMAT_D32_SFLOAT},
                                                         VK_IMAGE_TILING_OPTIMAL,
                                                         VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT |
                                                         VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT);
    //The two null context will get overwritten by the create  calls below
    std::span<VkImageView> viewsForMips = MemoryArena::AllocSpan<VkImageView>(allocationArena, HIZDEPTH);

    uint32_t depthWidth = previousPow2(initializedrenderer.swapchain.extent.width);
    uint32_t depthHeight = previousPow2(initializedrenderer.swapchain.extent.height);
    DepthPyramidInfo bufferInfo = {
        VK_FORMAT_R32_SFLOAT, VK_NULL_HANDLE, {}, viewsForMips, VmaAllocation{}, {depthWidth, depthHeight}
    };


    BufferCreationContext context = {
        .device = initializedrenderer.vkbdevice.device,
        .allocator = initializedrenderer.vmaAllocator,
        .rendererdeletionqueue = deletionQueue,
        .commandPoolmanager = commandPoolmanager
    };


    TextureUtilities::createImage(context, depthWidth, depthHeight, bufferInfo.format,
                                  VK_IMAGE_TILING_OPTIMAL,
                                  VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                  (bufferInfo.image),
                                  (bufferInfo.vmaAllocation), HIZDEPTH, 1, false, "DEPTH PYRAMID TEXTURE");
    for (int i = 0; i < bufferInfo.viewsForMips.size(); i++)
    {
        bufferInfo.viewsForMips[i] =
            TextureUtilities::createImageViewCustomMip(context, (bufferInfo.image), bufferInfo.format,
                                                       VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_VIEW_TYPE_2D, 1, 0, 1, i);
        SetDebugObjectName(initializedrenderer.vkbdevice.device, VK_OBJECT_TYPE_IMAGE, "depthPyramic image", (uint64_t)bufferInfo.image);
        SetDebugObjectName(initializedrenderer.vkbdevice.device, VK_OBJECT_TYPE_IMAGE_VIEW, "depthPyramid imageview for mip", (uint64_t)bufferInfo.viewsForMips[i]);
    }
    bufferInfo.view =
        TextureUtilities::createImageViewCustomMip(context, (bufferInfo.image), bufferInfo.format,
                                                   VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_VIEW_TYPE_2D, 1, 0, VK_REMAINING_MIP_LEVELS, 0);
    SetDebugObjectName(initializedrenderer.vkbdevice.device, VK_OBJECT_TYPE_IMAGE_VIEW, "depthPyramid imageview", (uint64_t)bufferInfo.view);

    auto tempBufferAndPool = commandPoolmanager->beginSingleTimeCommands(false);
    VkImageMemoryBarrier2 barrier11 = GetImageBarrier(bufferInfo.image,
                                                   0,
                                                   0,
                                                   VK_IMAGE_LAYOUT_UNDEFINED,
                                                   0,
                                                   0,
                                                   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                   VK_IMAGE_ASPECT_COLOR_BIT,
                                                   0,
                                                   HIZDEPTH);
    SetPipelineBarrier(tempBufferAndPool->buffer, 0, 0, nullptr, 1, &barrier11);

    commandPoolmanager->endSingleTimeCommands(tempBufferAndPool, true); //todo js sync

    return bufferInfo;
}

#pragma endregion


void createSemaphore(VkDevice device, VkSemaphore* semaphorePtr, const char* debugName,
                     RendererDeletionQueue* deletionQueue, bool pushToDeletionQueue)
{
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VK_CHECK(vkCreateSemaphore(device, &semaphoreInfo, nullptr, semaphorePtr));
    SetDebugObjectName(device, VK_OBJECT_TYPE_SEMAPHORE, debugName, uint64_t(*semaphorePtr));
    if (!pushToDeletionQueue)
    {
        return;
    }
    deletionQueue->push_backVk(deletionType::Semaphore, uint64_t(*semaphorePtr));
}

void static_createFence(VkDevice device, VkFence* fencePtr, const char* debugName, RendererDeletionQueue* deletionQueue)
{
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VK_CHECK(vkCreateFence(device, &fenceInfo, nullptr, fencePtr));
    deletionQueue->push_backVk(deletionType::Fence, uint64_t(*fencePtr));
    SetDebugObjectName(device, VK_OBJECT_TYPE_FENCE, debugName, uint64_t(*fencePtr));
}
VkImageMemoryBarrier2 AllTextureAccessBarrier(CommandBufferPoolQueue bandp, VkImage image,
    VkImageLayout oldLayout,
    VkImageLayout newLayout, uint32_t mipLevel, uint32_t levelCount)
{
    VkImageMemoryBarrier2 barrier11 = GetImageBarrier(image,
                                                VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT,
                                                VK_PIPELINE_STAGE_ALL_COMMANDS_BIT ,
                                                oldLayout,
                                                VK_PIPELINE_STAGE_ALL_COMMANDS_BIT ,
                                                VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT,
                                                newLayout,
                                                VK_IMAGE_ASPECT_COLOR_BIT,
                                                mipLevel,
                                                levelCount);
    SetPipelineBarrier(bandp->buffer, 0, 0, nullptr, 1, &barrier11);
    return barrier11;
}

//zoux code
VkBufferMemoryBarrier2 bufferBarrier(VkBuffer buffer, VkPipelineStageFlags2 srcStageMask, VkAccessFlags2 srcAccessMask,
                                     VkPipelineStageFlags2 dstStageMask, VkAccessFlags2 dstAccessMask)
{
    VkBufferMemoryBarrier2 result = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2};

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
void SetPipelineBarrier(VkCommandBuffer commandBuffer, VkDependencyFlags dependencyFlags, size_t bufferBarrierCount,
                     const VkBufferMemoryBarrier2* bufferBarriers, size_t imageBarrierCount,
                     const VkImageMemoryBarrier2* imageBarriers)
{
    VkDependencyInfo dependencyInfo = {VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    dependencyInfo.dependencyFlags = dependencyFlags;
    dependencyInfo.bufferMemoryBarrierCount = static_cast<unsigned>(bufferBarrierCount);
    dependencyInfo.pBufferMemoryBarriers = bufferBarriers;
    dependencyInfo.imageMemoryBarrierCount = static_cast<unsigned>(imageBarrierCount);
    dependencyInfo.pImageMemoryBarriers = imageBarriers;

    vkCmdPipelineBarrier2(commandBuffer, &dependencyInfo);
}


void PipelineMemoryBarrier(VkCommandBuffer commandBuffer, VkPipelineStageFlags2 srcStageMask, VkAccessFlags2 srcAccessMask,
                                     VkPipelineStageFlags2 dstStageMask, VkAccessFlags2 dstAccessMask)
{
    VkMemoryBarrier2 membar =
        {
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .pNext = {},
        .srcStageMask = srcStageMask,
          .srcAccessMask = srcAccessMask,
          .dstStageMask = dstStageMask,
          .dstAccessMask = dstAccessMask
        };

    VkDependencyInfo dependencyInfo = {VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    dependencyInfo.memoryBarrierCount = 1;
    dependencyInfo.pMemoryBarriers = &membar;

    vkCmdPipelineBarrier2(commandBuffer, &dependencyInfo);
}

//zoux code
VkImageMemoryBarrier2 GetImageBarrier(VkImage image, VkPipelineStageFlags2 srcStageMask, VkAccessFlags2 srcAccessMask,
                                   VkImageLayout oldLayout, VkPipelineStageFlags2 dstStageMask,
                                   VkAccessFlags2 dstAccessMask, VkImageLayout newLayout, VkImageAspectFlags aspectMask,
                                   uint32_t baseMipLevel, uint32_t levelCount, uint32_t baselayer)
{
    VkImageMemoryBarrier2 result = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};

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
    result.subresourceRange.baseArrayLayer = baselayer;
    result.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

    return result;
}

void AddBufferTrasnfer(VkBuffer sourceBuffer, VkBuffer targetBuffer, size_t copySize, VkCommandBuffer cmdBuffer)
{
    VkBufferMemoryBarrier bufMemBarrier = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
    bufMemBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
    bufMemBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    bufMemBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufMemBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufMemBarrier.buffer = sourceBuffer;
    bufMemBarrier.offset = 0;
    bufMemBarrier.size = VK_WHOLE_SIZE;
    vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 0, nullptr, 1, &bufMemBarrier, 0, nullptr);


    //Copy vertex data over
    BufferUtilities::copyBufferWithCommandBuffer(cmdBuffer, sourceBuffer,
                                                 targetBuffer, copySize);
    
    VkBufferMemoryBarrier bufMemBarrier2 = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
    bufMemBarrier2.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    bufMemBarrier2.dstAccessMask =  VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_2_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT; // We created a uniform buffer
    bufMemBarrier2.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufMemBarrier2.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufMemBarrier2.buffer = targetBuffer;
    bufMemBarrier2.offset = 0;
    bufMemBarrier2.size = VK_WHOLE_SIZE;
    vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
                         0, 0, nullptr, 1, &bufMemBarrier2, 0, nullptr);
}

void TransitionImageForRendering(PerThreadRenderContext context, ActiveRenderStepData* RenderStepContext, VkImage image,
                                 VkImageLayout layoutIn, VkImageLayout layoutOut, VkPipelineStageFlags* waitStages,
                                 int mipCount, bool depth)
{
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = 0; // Optional
    beginInfo.pInheritanceInfo = nullptr; // Optional
    //Transition swapchain for rendering

    TextureUtilities::transitionImageLayout(objectCreationContextFromRendererContext(context), image,
                                            static_cast<VkFormat>(0), layoutIn, layoutOut,
                                            RenderStepContext->commandBuffer, mipCount, false, depth);



    ///////////////////////// Transition swapChain  />
}
