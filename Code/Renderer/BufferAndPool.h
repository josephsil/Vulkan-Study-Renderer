#pragma once
#include <vector>
#include "VulkanIncludes/forward-declarations-renderer.h"

struct bufferAndPool
{
    VkCommandBuffer buffer;
    VkCommandPool pool;
    VkQueue queue;
    std::vector<VkSemaphore> waitSemaphores = {};
    std::vector<VkSemaphore> signalSemaphores = {};
};
