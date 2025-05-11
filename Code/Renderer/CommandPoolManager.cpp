#include "CommandPoolManager.h"

#include "RendererDeletionQueue.h"
#include "rendererGlobals.h"
#include "./VulkanIncludes/Vulkan_Includes.h"
#include "VkBootstrap.h" //TODO JS: dont love vkb being in multiple places
#include "MainRenderer/VulkanRendererInternals/RendererHelpers.h"


void createCommandPool(VkDevice device, uint32_t index, VkCommandPool* pool);
static QueueData QUEUES; //todo js, move, guard against vk null

void INIT_QUEUES(vkb::Device device)
{
        //Get queues -- the dedicated transfer queue will prevent this from running on many gpus
        auto graphicsqueueResult = device.get_queue(vkb::QueueType::graphics);
        auto presentqueueResult = device.get_queue(vkb::QueueType::present);
        auto transferqueueResult = device.get_dedicated_queue(vkb::QueueType::transfer);
        if (!transferqueueResult)
        {
            printf("NO DEDICATED TRANSFER QUEUE \n");
            exit(1);
        }
        auto compute_ret = device.get_queue(vkb::QueueType::compute);

   
              QUEUES.graphicsQueue = graphicsqueueResult.value();
            QUEUES.graphicsQueueFamily = device.get_queue_index(vkb::QueueType::graphics).value();
              QUEUES.presentQueue = presentqueueResult.value();
            QUEUES.presentQueueFamily = device.get_queue_index(vkb::QueueType::present).value();
              QUEUES.transferQueue = transferqueueResult.value();
            QUEUES.transferQueueFamily = device.get_dedicated_queue_index(vkb::QueueType::transfer).value();
              QUEUES.computeQueue = compute_ret.value();
            QUEUES.computeQueueFamily = device.get_queue_index(vkb::QueueType::compute).value();
        };

QueueData* GET_QUEUES()
{
    return &QUEUES;
}


CommandPoolManager::CommandPoolManager(vkb::Device vkbdevice, RendererDeletionQueue* deletionQueue)
{
    // Queues = GET_QUEUES(vkbdevice);
    device = vkbdevice.device;
    this->deletionQueue = deletionQueue;
    MemoryArena::initialize(&this->arena, 6000 * 10); // tood js
    createCommandPool(device, QUEUES.graphicsQueueFamily, &commandPool);
    createCommandPool(device, QUEUES.transferQueueFamily, &transferCommandPool);

    // deletionQueue->push_backVk(deletionType::CommandPool, uint64_t(commandPool));
    // deletionQueue->push_backVk(deletionType::CommandPool, uint64_t(transferCommandPool));
}

CommandBufferPoolQueue CommandPoolManager::beginSingleTimeCommands_transfer()
{
    return beginSingleTimeCommands(true);
}

CommandBufferPoolQueue CommandPoolManager::beginSingleTimeCommands(
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

    VkFence outputFence;
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VK_CHECK(vkCreateFence(device, &fenceInfo, nullptr, &outputFence));
    setDebugObjectName(device, VK_OBJECT_TYPE_FENCE, "begin single time commands fence", uint64_t(outputFence));


    setDebugObjectName(device, VK_OBJECT_TYPE_COMMAND_BUFFER, "Unnamed command buffer from beginsingl;etime", (uint64_t)commandBuffer);
    CommandBufferPoolQueue result = MemoryArena::Alloc<CommandBufferPoolQueue_T>(&arena);
    *result = {
        commandBuffer, useTransferPoolInsteadOfGraphicsPool ? transferCommandPool : commandPool,
        useTransferPoolInsteadOfGraphicsPool ? QUEUES.transferQueue : QUEUES.graphicsQueue, outputFence
    };
    return result;
}

void CommandPoolManager::endSingleTimeCommands(VkCommandBuffer buffer, VkFence fence)
{
    vkEndCommandBuffer(buffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &buffer;

    QUEUES.transferQueueMutex.lock(); //todo js
        if (fence != VK_NULL_HANDLE)
    {vkWaitForFences(device, 1, &fence,VK_TRUE, SIZE_MAX);}
    vkQueueSubmit(QUEUES.transferQueue, 1, &submitInfo, fence);

    vkQueueWaitIdle(QUEUES.transferQueue);

    QUEUES.transferQueueMutex.unlock(); //todo js
    vkFreeCommandBuffers(device, transferCommandPool, 1, &buffer);
}

void CommandPoolManager::endSingleTimeCommands(CommandBufferPoolQueue bufferAndPool)
{
    vkEndCommandBuffer(bufferAndPool->buffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &bufferAndPool->buffer;

    QUEUES.graphicsQueueMutex.lock(); //todo js
    QUEUES.transferQueueMutex.lock(); //todo js
    
    vkWaitForFences(device, 1, &bufferAndPool->fence,VK_TRUE, SIZE_MAX);
    vkResetFences(device, 1, &bufferAndPool->fence);
    vkQueueSubmit(bufferAndPool->queue, 1, &submitInfo, bufferAndPool->fence);

    
    // vkQueueWaitIdle(bufferAndPool->queue);
    
    QUEUES.graphicsQueueMutex.unlock(); //todo js
    QUEUES.transferQueueMutex.unlock(); //todo js

    deletionQueue->push_backVk(deletionType::CommandBuffer, (uint64_t)bufferAndPool); //todo js sketchy
    deletionQueue->push_backVk(deletionType::WAITFORANDDELETEFENCE, (uint64_t)bufferAndPool->fence); //Wait for fence before freeing anything we allocated beforehand
    // vkFreeCommandBuffers(device, bufferAndPool->pool, 1, &bufferAndPool->buffer);
}

void createCommandPool(VkDevice device, uint32_t index, VkCommandPool* pool)
{
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = index;

    VK_CHECK(vkCreateCommandPool(device, &poolInfo, nullptr, pool));
}

