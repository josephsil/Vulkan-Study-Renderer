#include "CommandPoolManager.h"

#include <iostream>

#include "Vulkan_Includes.h";
#include "VkBootstrap.h"; //TODO JS: dont love vkb being in multiple places

QueueData GET_QUEUES(vkb::Device device);
void createCommandPool(VkDevice device, uint32_t index, VkCommandPool* pool);

CommandPoolManager::CommandPoolManager()
{
};

CommandPoolManager::CommandPoolManager(vkb::Device vkbdevice)
{
    Queues = GET_QUEUES(vkbdevice);
    device = vkbdevice.device;
    createCommandPool(device, Queues.graphicsQueueFamily, &commandPool);
    createCommandPool(device, Queues.transferQueueFamily, &transferCommandPool);
}

VkCommandBuffer CommandPoolManager::beginSingleTimeCommands_transfer()
{
    return beginSingleTimeCommands(true).buffer;
}

bufferAndPool CommandPoolManager::beginSingleTimeCommands(
    bool useTransferPoolInsteadOfGraphicsPool)
{
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = useTransferPoolInsteadOfGraphicsPool ? transferCommandPool : commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    bufferAndPool result = {
        commandBuffer, useTransferPoolInsteadOfGraphicsPool ? transferCommandPool : commandPool,
        useTransferPoolInsteadOfGraphicsPool ? Queues.transferQueue : Queues.graphicsQueue
    };
    return result;
}

void CommandPoolManager::endSingleTimeCommands(VkCommandBuffer buffer)
{
    vkEndCommandBuffer(buffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &buffer;

    vkQueueSubmit(Queues.transferQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(Queues.transferQueue);

    vkFreeCommandBuffers(device, transferCommandPool, 1, &buffer);
}

void CommandPoolManager::endSingleTimeCommands(bufferAndPool bufferAndPool)
{
    vkEndCommandBuffer(bufferAndPool.buffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &bufferAndPool.buffer;

    vkQueueSubmit(bufferAndPool.queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(bufferAndPool.queue);

    vkFreeCommandBuffers(device, bufferAndPool.pool, 1, &bufferAndPool.buffer);
}

void createCommandPool(VkDevice device, uint32_t index, VkCommandPool* pool)
{
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = index;

    VK_CHECK(vkCreateCommandPool(device, &poolInfo, nullptr, pool));
}


QueueData GET_QUEUES(vkb::Device device)
{
    //Get queues -- the dedicated transfer queue will prevent this from running on many gpus
    auto graphicsqueueResult = device.get_queue(vkb::QueueType::graphics);
    auto presentqueueResult = device.get_queue(vkb::QueueType::present);
    auto transferqueueResult = device.get_dedicated_queue(vkb::QueueType::transfer);
    if (!transferqueueResult)
    {
        std::cerr << ("NO DEDICATED TRANSFER QUEUE");
        exit(1);
    }
    auto compute_ret = device.get_queue(vkb::QueueType::compute);

    return {
        graphicsqueueResult.value(),
        device.get_queue_index(vkb::QueueType::graphics).value(),
        presentqueueResult.value(),
        device.get_queue_index(vkb::QueueType::present).value(),
        transferqueueResult.value(),
        device.get_dedicated_queue_index(vkb::QueueType::transfer).value(),
        compute_ret.value(),
        device.get_queue_index(vkb::QueueType::compute).value()
    };
}
