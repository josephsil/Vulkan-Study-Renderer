#pragma once
#include <cstdint>



#include "vulkan-forwards.h"
#include "common-structs.h"

namespace vkb
{
    struct Device;
}

struct bufferAndPool; 

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


struct CommandPoolManager
{
public:
    QueueData Queues;
    CommandPoolManager();
    CommandPoolManager(vkb::Device device);

    VkCommandPool commandPool;
    VkCommandPool transferCommandPool;
    VkCommandBuffer beginSingleTimeCommands_transfer();
    bufferAndPool beginSingleTimeCommands(bool useTransferPool); //TODO JS: can i move to cpp?

    void endSingleTimeCommands(VkCommandBuffer buffer);
    void endSingleTimeCommands(bufferAndPool commandBuffer);

private:
    VkDevice device;
};
