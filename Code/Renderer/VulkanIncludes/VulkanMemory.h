#pragma once

#include "forward-declarations-renderer.h"

namespace VulkanMemory
{
    VmaAllocator GetAllocator(VkDevice device, VkPhysicalDevice
                              physicalDevice, VkInstance instance);


    void DestroyBuffer(VmaAllocator allocator, VkBuffer buffer, VmaAllocation allocation);
    void DestroyImage(VmaAllocator allocator, VkImage image, VmaAllocation allocation);
    void MapMemory(VmaAllocator allocator, VmaAllocation allocation, void** data);
    void UnmapMemory(VmaAllocator allocator, VmaAllocation allocation);
}
