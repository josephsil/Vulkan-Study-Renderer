#pragma once
#include <cstdint>
#include <vulkan/vulkan_core.h>

#include "RendererDeletionQueue.h"
#include "VkBootstrap.h"
#include "General/MemoryArena.h"
// #include "rendererGlobals.h"


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
typedef CommandBufferPoolQueue_T* CommandBufferPoolQueue; 


struct QueueData
{
    
    
    VkQueue graphicsQueue = VK_NULL_HANDLE; //TODO JS: Currently wrapped in atomics for per-thread synchronization. Need t o move to a 'submit thread' model where we only submit from one place
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
    
};

QueueData* GET_QUEUES(); // TODO JS;
void INIT_QUEUES(vkb::Device d); // TODO JS;
//This is silly -- merge with the apporoach used for drawing
class CommandPoolManager
{
public:


    CommandPoolManager(vkb::Device vkbdevice, RendererDeletionQueue* deletionQueue);

    VkCommandPool commandPool;
    VkCommandPool transferCommandPool;
    CommandBufferPoolQueue beginSingleTimeCommands_transfer();
    CommandBufferPoolQueue beginSingleTimeCommands(bool useTransferPoolInsteadOfGraphicsPool); //TODO JS: can i move to cpp?
    void endSingleTimeCommands(VkCommandBuffer buffer, VkFence fence);

    void endSingleTimeCommands(CommandBufferPoolQueue bufferAndPool, bool waitForFence = false);

private:
    VkDevice device;
    MemoryArena::memoryArena arena;
    RendererDeletionQueue* deletionQueue;
};
