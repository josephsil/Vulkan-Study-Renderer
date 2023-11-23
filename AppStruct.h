#pragma once
#include "vulkan-forwards.h"
struct CommandPoolManager;

//This is stuff that I (currently) have to pass around a lot
struct RendererHandles
{
    VkPhysicalDevice physicalDevice;
    VkDevice device; //Logical device
    CommandPoolManager* commandPoolmanager;
    VmaAllocator allocator;
};