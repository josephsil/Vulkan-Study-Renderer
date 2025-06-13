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
    MemoryArena::memoryArena arena;
    RendererDeletionQueue* deletionQueue;
};
