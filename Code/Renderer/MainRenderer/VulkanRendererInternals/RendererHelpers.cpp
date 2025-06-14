#include "RendererHelpers.h"

#include <Renderer/VulkanBuffers/bufferCreation.h>
#include <Renderer/PerThreadRenderContext.h>
#include <Renderer/TextureCreation/Internal/TextureCreationUtilities.h>
#include <Renderer/vulkan-utilities.h>

#include <fmtInclude.h>
#pragma region depth

inline uint32_t PreviousPow2(uint32_t v)
{
	return std::bit_floor(v) ;
}

//TODO: Reuse more code between CreateDepthResources and CreateDepthPyramidResources
DepthBufferInfo CreateDepthResources(rendererObjects initializedrenderer,
											MemoryArena::Allocator* allocationArena,
											RendererDeletionQueue* deletionQueue,
											CommandPoolManager* commandPoolmanager)
{
	auto depthFormat = Capabilities::FindDepthFormat(initializedrenderer.vkbPhysicalDevice.physical_device);
	BufferCreationContext context = {
		.device = initializedrenderer.vkbdevice.device,
		.allocator = initializedrenderer.vmaAllocator,
		.rendererdeletionqueue = deletionQueue,
		.commandPoolmanager = commandPoolmanager
		};

	VkImage image = {};
    
	VmaAllocation alloc;

	TextureUtilities::createImage(context, initializedrenderer.swapchain.extent.width,
								  initializedrenderer.swapchain.extent.height, depthFormat,
								  VK_IMAGE_TILING_OPTIMAL,
								  VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
								  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
								  (image),
								  (alloc), 1, 1, false, "DEPTH TEXTURE");


	VkImageView view = TextureUtilities::createImageViewCustomMip(context, (image), depthFormat,
																  VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_VIEW_TYPE_2D, 1, 0,
																  1, 0);
   
	SetDebugObjectNameS(initializedrenderer.vkbdevice.device, 
						VK_OBJECT_TYPE_IMAGE, "DepthBufferInfo image", (uint64_t)image);
	SetDebugObjectNameS(initializedrenderer.vkbdevice.device, 
						VK_OBJECT_TYPE_IMAGE_VIEW, "DepthBufferInfo imageview", (uint64_t)view);

	return { .format = depthFormat, .image = image, .view = view, .vmaAllocation = alloc };
}

DepthPyramidInfo CreateDepthPyramidResources(rendererObjects initializedrenderer,
													MemoryArena::Allocator* allocationArena,
													const char* name,
													RendererDeletionQueue* deletionQueue,
													CommandPoolManager* commandPoolmanager)
{
	auto depthFormat = Capabilities::FindSupportedFormat(initializedrenderer.vkbPhysicalDevice.physical_device,
														 { VK_FORMAT_D32_SFLOAT, VK_FORMAT_R32_SFLOAT, VK_FORMAT_D16_UNORM },
														 VK_IMAGE_TILING_OPTIMAL,
														 VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT |
															 VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT);

	std::span<VkImageView> viewsForMips = MemoryArena::AllocSpan<VkImageView>(allocationArena, HIZDEPTH);

	uint32_t depthWidth = PreviousPow2(initializedrenderer.swapchain.extent.width);
	uint32_t depthHeight = PreviousPow2(initializedrenderer.swapchain.extent.height);
	uint32_t depthSquareMax = glm::max(depthWidth, depthHeight);
	DepthPyramidInfo bufferInfo = {
		VK_FORMAT_R32_SFLOAT, {},
		{}, viewsForMips, VmaAllocation{}, { depthSquareMax, depthSquareMax }
	};


	BufferCreationContext context = {
		.device = initializedrenderer.vkbdevice.device,
		.allocator = initializedrenderer.vmaAllocator,
		.rendererdeletionqueue = deletionQueue,
		.commandPoolmanager = commandPoolmanager
		};


	TextureUtilities::createImage(context, depthSquareMax, depthSquareMax, bufferInfo.format,
								  VK_IMAGE_TILING_OPTIMAL,
								  VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
								  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
								  (bufferInfo.image),
								  (bufferInfo.vmaAllocation), HIZDEPTH, 1, false, "DEPTH PYRAMID TEXTURE");

	auto cursor = MemoryArena::GetCurrentOffset(allocationArena);
	auto scratchBuffer = MemoryArena::AllocSpan<char>(allocationArena, 256);
	for (int i = 0; i < bufferInfo.viewsForMips.size(); i++)
	{
		bufferInfo.viewsForMips[i] =
			TextureUtilities::createImageViewCustomMip(context, (bufferInfo.image), bufferInfo.format,
													   VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_VIEW_TYPE_2D, 1, 0, 1, i);
		fmt::format_to_n(scratchBuffer.begin(),  scratchBuffer.size(), "{} Pyramid View Level{}\0", name, i);
		SetDebugObjectName(initializedrenderer.vkbdevice.device, VK_OBJECT_TYPE_IMAGE_VIEW, scratchBuffer.data(), (uint64_t)bufferInfo.viewsForMips[i]);
	}
	fmt::format_to_n(scratchBuffer.begin(),  scratchBuffer.size(), "{} Pyramid View Alllevels\0", name);
	bufferInfo.view =
		TextureUtilities::createImageViewCustomMip(context, (bufferInfo.image), bufferInfo.format,
												   VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_VIEW_TYPE_2D, 1, 0, VK_REMAINING_MIP_LEVELS, 0);
	SetDebugObjectNameS(initializedrenderer.vkbdevice.device, VK_OBJECT_TYPE_IMAGE_VIEW, scratchBuffer.data(), (uint64_t)bufferInfo.view);

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
	SetPipelineBarrier(tempBufferAndPool->buffer, {}, {&barrier11, 1 });

	commandPoolmanager->endSingleTimeCommands(tempBufferAndPool, true); //todo js sync

	MemoryArena::FreeToOffset(allocationArena,cursor); //Free our scratch memory.
	return bufferInfo;
}

#pragma endregion


void CreateSemaphore(VkDevice device, VkSemaphore* semaphorePtr, const char* debugName,
                     RendererDeletionQueue* deletionQueue, bool pushToDeletionQueue)
{
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VK_CHECK(vkCreateSemaphore(device, &semaphoreInfo, nullptr, semaphorePtr));
    SetDebugObjectName(device, VK_OBJECT_TYPE_SEMAPHORE, const_cast<char*>(debugName), uint64_t(*semaphorePtr));
    if (!pushToDeletionQueue)
    {
        return;
    }
    deletionQueue->push_backVk(deletionType::Semaphore, uint64_t(*semaphorePtr));
}

void CreateFence(VkDevice device, VkFence* fencePtr, const char* debugName, RendererDeletionQueue* deletionQueue)
{
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VK_CHECK(vkCreateFence(device, &fenceInfo, nullptr, fencePtr));
    deletionQueue->push_backVk(deletionType::Fence, uint64_t(*fencePtr));
    SetDebugObjectName(device, VK_OBJECT_TYPE_FENCE, const_cast<char*>(debugName), uint64_t(*fencePtr));
}

VkImageMemoryBarrier2 AllTextureAccessBarrier(CommandBufferData bufferData, VkImage image,
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
	SetPipelineBarrier(bufferData->buffer, {}, { &barrier11, 1});
    return barrier11;
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
    BufferUtilities::CopyBuffer(cmdBuffer, sourceBuffer,
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

void SetPipelineBarrier(VkCommandBuffer commandBuffer,  
						std::span<VkBufferMemoryBarrier2> bufferBarriers,
						std::span<VkImageMemoryBarrier2> imageBarriers)
{
	VkDependencyInfo dependencyInfo = {};
	dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependencyInfo.dependencyFlags = 0;
    dependencyInfo.bufferMemoryBarrierCount = (uint32_t)bufferBarriers.size();
    dependencyInfo.pBufferMemoryBarriers = bufferBarriers.data();

    dependencyInfo.imageMemoryBarrierCount = (uint32_t)imageBarriers.size();
    dependencyInfo.pImageMemoryBarriers = imageBarriers.data();

    vkCmdPipelineBarrier2(commandBuffer, &dependencyInfo);
}


//The following three functions are based on code from Niagara 
//https://github.com/zeux/niagara/tree/master?tab=readme-ov-file 
/*MIT License

	Copyright (c) 2018 Arseny Kapoulkine

	Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

	The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
//From Niagara https://github.com/zeux/niagara/tree/master?tab=readme-ov-file 


//From Niagara https://github.com/zeux/niagara/tree/master?tab=readme-ov-file 
VkImageMemoryBarrier2 GetImageBarrier(VkImage image, VkPipelineStageFlags2 srcStageMask, VkAccessFlags2 srcAccessMask,
									  VkImageLayout oldLayout, VkPipelineStageFlags2 dstStageMask,
									  VkAccessFlags2 dstAccessMask, VkImageLayout newLayout, VkImageAspectFlags aspectMask,
									  uint32_t baseMipLevel, uint32_t levelCount, uint32_t baselayer, uint32_t layerCount)
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
    result.subresourceRange.layerCount = layerCount == UINT32_MAX ? VK_REMAINING_ARRAY_LAYERS : layerCount;

    return result;
}

//From Niagara https://github.com/zeux/niagara/tree/master?tab=readme-ov-file 
VkBufferMemoryBarrier2 GetBufferBarrier(VkBuffer buffer, VkPipelineStageFlags2 srcStageMask, VkAccessFlags2 srcAccessMask,
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

