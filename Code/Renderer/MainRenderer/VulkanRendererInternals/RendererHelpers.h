#pragma once 
#include <General/GLM_IMPL.h>

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
DepthBufferInfo static_createDepthResources(rendererObjects initializedrenderer,
                                            MemoryArena::memoryArena* allocationArena,
                                            RendererDeletionQueue* deletionQueue,
                                            CommandPoolManager* commandPoolmanager);
DepthPyramidInfo static_createDepthPyramidResources(rendererObjects initializedrenderer,
                                                    MemoryArena::memoryArena* allocationArena,
                                                    RendererDeletionQueue* deletionQueue,
                                                    CommandPoolManager* commandPoolmanager);

//Sync objects
void static_createFence(VkDevice device, VkFence* fencePtr, const char* debugName,
                        RendererDeletionQueue* deletionQueue);
void createSemaphore(VkDevice device, VkSemaphore* semaphorePtr, const char* debugName,
                     RendererDeletionQueue* deletionQueue);

//Utilities/helpers
void SetPipelineBarrier(VkCommandBuffer commandBuffer, VkDependencyFlags dependencyFlags, size_t bufferBarrierCount,
                     const VkBufferMemoryBarrier2* bufferBarriers, size_t imageBarrierCount,
                     const VkImageMemoryBarrier2* imageBarriers);
void PipelineMemoryBarrier(VkCommandBuffer commandBuffer, VkPipelineStageFlags2 srcStageMask, VkAccessFlags2 srcAccessMask,
                                     VkPipelineStageFlags2 dstStageMask, VkAccessFlags2 dstAccessMask);

VkImageMemoryBarrier2 AllTextureAccessBarrier(CommandBufferPoolQueue bandp, VkImage image,
    VkImageLayout oldLayout,
    VkImageLayout newLayout, uint32_t mipLevel, uint32_t levelCount = 1);
VkBufferMemoryBarrier2 bufferBarrier(VkBuffer buffer, VkPipelineStageFlags2 srcStageMask, VkAccessFlags2 srcAccessMask,
                                     VkPipelineStageFlags2 dstStageMask, VkAccessFlags2 dstAccessMask);
VkImageMemoryBarrier2 GetImageBarrier(VkImage image, VkPipelineStageFlags2 srcStageMask, VkAccessFlags2 srcAccessMask,
                                   VkImageLayout oldLayout, VkPipelineStageFlags2 dstStageMask,
                                   VkAccessFlags2 dstAccessMask, VkImageLayout newLayout, VkImageAspectFlags aspectMask,
                                   uint32_t baseMipLevel, uint32_t levelCount);

//rendering submisison


void AddBufferTrasnfer(VkBuffer sourceBuffer, VkBuffer targetBuffer, size_t copySize, VkCommandBuffer cmdBuffer);
void TransitionImageForRendering(PerThreadRenderContext context, ActiveRenderStepData* RenderStepContext, VkImage image,
                                 VkImageLayout layoutIn, VkImageLayout layoutOut, VkPipelineStageFlags* waitStages,
                                 int mipCount, bool depth);
