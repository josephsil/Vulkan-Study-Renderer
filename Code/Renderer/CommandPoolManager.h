#pragma once
#include <cstdint>

#include "RendererDeletionQueue.h"
#include "VulkanIncludes/forward-declarations-renderer.h"
// #include "rendererGlobals.h"

namespace vkb
{
    struct Device;
}

struct CommandBufferPoolQueue
{
    VkCommandBuffer buffer;
    VkCommandPool pool;
    VkQueue queue;
};

struct QueueData
{
    VkQueue graphicsQueue;
    uint32_t graphicsQueueFamily;
    VkQueue presentQueue;
    uint32_t presentQueueFamily;
    VkQueue transferQueue;
    uint32_t transferQueueFamily;
    VkQueue computeQueue;
    uint32_t computeQueueFamily;
};


//This is silly -- merge with the apporoach used for drawing
class CommandPoolManager
{
public:
    QueueData Queues;
    CommandPoolManager(vkb::Device vkbdevice, RendererDeletionQueue* deletionQueue);

    VkCommandPool commandPool;
    VkCommandPool transferCommandPool;
    VkCommandBuffer beginSingleTimeCommands_transfer();
    CommandBufferPoolQueue beginSingleTimeCommands(bool useTransferPoolInsteadOfGraphicsPool); //TODO JS: can i move to cpp?

    void endSingleTimeCommands(VkCommandBuffer buffer);
    void endSingleTimeCommands(CommandBufferPoolQueue commandBuffer);

private:
    VkDevice device;
};
