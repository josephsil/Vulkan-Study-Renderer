#pragma once
#include <cstdint>
#include <vulkan/vulkan_core.h>

#include "RendererDeletionQueue.h"
#include "VkBootstrap.h"
#include "General/MemoryArena.h"


namespace vkb
{
    struct Device;
}

//This wrapper for command buffers is used requently throughout the renderer and in texture import (as CommandBufferData)
//Based on a pattern from http://vulkan-tutorial.com/ which I don't think I want to carry forward
//This class also acquired some helpers for queue access and lifetime.
struct CommandBufferPoolQueue_T
{
    VkCommandBuffer buffer;
    VkCommandPool pool;
    VkQueue queue;
    VkFence fence;
};
typedef CommandBufferPoolQueue_T* CommandBufferData; 


struct QueueData
{
    
    
    VkQueue graphicsQueue = VK_NULL_HANDLE; 
    uint32_t graphicsQueueFamily = UINT32_MAX;
    std::mutex graphicsQueueMutex = std::mutex();
    
     VkQueue presentQueue = VK_NULL_HANDLE;
    uint32_t presentQueueFamily = UINT32_MAX;
    std::mutex presentQueueMutex = std::mutex();
    
     VkQueue transferQueue = VK_NULL_HANDLE;
    uint32_t transferQueueFamily = UINT32_MAX;
    std::mutex transferQueueMutex = std::mutex();
    
     VkQueue computeQueue = VK_NULL_HANDLE;
    uint32_t computeQueueFamily = UINT32_MAX;
    std::mutex computeQueueMutex = std::mutex();
    //
};

QueueData* GET_QUEUES(); 
void INIT_QUEUES(vkb::Device d); 
//This is (mostly) older code, needs a rethink.
//Wraps command buffer creation and queue management. Kinda a bad fit.
class CommandPoolManager
{
public:


    CommandPoolManager(vkb::Device vkbdevice, RendererDeletionQueue* deletionQueue);

    VkCommandPool commandPool;
    VkCommandPool transferCommandPool;
    CommandBufferData beginSingleTimeCommands_transfer();
    CommandBufferData beginSingleTimeCommands(bool useTransferPoolInsteadOfGraphicsPool); 
    void endSingleTimeCommands(VkCommandBuffer buffer, VkFence fence);
    void endSingleTimeCommands(CommandBufferData bufferAndPool, bool waitForFence = false);

    VkDevice device;
    MemoryArena::Allocator arena;
    RendererDeletionQueue* deletionQueue;
};
