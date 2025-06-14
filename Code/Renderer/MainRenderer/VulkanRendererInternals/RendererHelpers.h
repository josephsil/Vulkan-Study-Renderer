#pragma once 
#include <General/GLM_impl.h>

#include "../../CommandPoolManager.h"
#include "../../gpu-data-structs.h"
#include <General/MemoryArena.h>
#include "../../Shaders/ShaderLoading.h"
#include "VkBootstrap.h"
#include "Scene/Scene.h"
#include "../rendererStructs.h"
#include "Renderer/RendererDeletionQueue.h"


struct ActiveRenderStepData;
//Depth resources
DepthBufferInfo CreateDepthResources(rendererObjects initializedrenderer,
                                            MemoryArena::Allocator* allocationArena,
                                            RendererDeletionQueue* deletionQueue,
                                            CommandPoolManager* commandPoolmanager);
DepthPyramidInfo CreateDepthPyramidResources(rendererObjects initializedrenderer,
                                                    MemoryArena::Allocator* allocationArena,
                                                    const char* name,
                                                    RendererDeletionQueue* deletionQueue,
                                                    CommandPoolManager* commandPoolmanager);

//Sync objects
void CreateFence(VkDevice device, VkFence* fencePtr, const char* debugName,
                        RendererDeletionQueue* deletionQueue);
void CreateSemaphore(VkDevice device, VkSemaphore* semaphorePtr, const char* debugName,
                     RendererDeletionQueue* deletionQueue,  bool pushToDeletionQueue = true);

//Utilities/helpers
void SetPipelineBarrier(VkCommandBuffer commandBuffer,  
							 std::span<VkBufferMemoryBarrier2> bufferBarriers,
						std::span<VkImageMemoryBarrier2> imageBarriers);
void PipelineMemoryBarrier(VkCommandBuffer commandBuffer, VkPipelineStageFlags2 srcStageMask, VkAccessFlags2 srcAccessMask,
                                     VkPipelineStageFlags2 dstStageMask, VkAccessFlags2 dstAccessMask);

VkImageMemoryBarrier2 AllTextureAccessBarrier(CommandBufferData bandp, VkImage image,
    VkImageLayout oldLayout,
    VkImageLayout newLayout, uint32_t mipLevel, uint32_t levelCount = 1);
VkBufferMemoryBarrier2 GetBufferBarrier(VkBuffer buffer, VkPipelineStageFlags2 srcStageMask, VkAccessFlags2 srcAccessMask,
                                     VkPipelineStageFlags2 dstStageMask, VkAccessFlags2 dstAccessMask);
VkImageMemoryBarrier2 GetImageBarrier(VkImage image, VkPipelineStageFlags2 srcStageMask, VkAccessFlags2 srcAccessMask,
                                   VkImageLayout oldLayout, VkPipelineStageFlags2 dstStageMask,
                                   VkAccessFlags2 dstAccessMask, VkImageLayout newLayout, VkImageAspectFlags aspectMask,
									  uint32_t baseMipLevel, uint32_t levelCount, uint32_t baselayer = 0, uint32_t layerCount = UINT32_MAX);

//rendering submisison


void AddBufferTrasnfer(VkBuffer sourceBuffer, VkBuffer targetBuffer, size_t copySize, VkCommandBuffer cmdBuffer);
