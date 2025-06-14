#pragma once
#include <Renderer/VulkanIncludes/forward-declarations-renderer.h>

#include "Renderer/CommandPoolManager.h"

//Forward declaration
struct Vertex;

//Helpers for creating vullkan buffers
namespace BufferUtilities
{
    void CreateDeviceBuffer(VmaAllocator allocator, const char* name, VkDeviceSize size,
                            VkBufferUsageFlags usage, VkDevice device, VmaAllocation* allocation, VkBuffer* buffer);
    void CreateImage(
        VkImageCreateInfo* pImageCreateInfo,
        VkImage* pImage, VmaAllocation* pAllocation, VkDevice device, VmaAllocator allocator,
        const char* debugName = "Bufferutiltiies create image vkimage");

    void CopyBuffer(VkCommandBuffer cb, VkBuffer srcBuffer, VkBuffer dstBuffer,
                                     VkDeviceSize size);

    void* CreateHostMappedBuffer(VmaAllocator allocator, VkDeviceSize size,
                                 VkBufferUsageFlags usage, VmaAllocation* allocation, VkBuffer& buffer);

    void CreateDeviceBuffer(VmaAllocator allocator, VkDeviceSize size, VkBufferUsageFlags usage,
                            VmaAllocation* allocation, VkBuffer& buffer);
    void CreateStagingBuffer(VkDeviceSize size,
                             VmaAllocation* allocation, VkBuffer& stagingBuffer, VkDevice device,
                             VmaAllocator allocator,
                             const char* debugName = "Bufferutiltiies create staging buffer buffer");
}
