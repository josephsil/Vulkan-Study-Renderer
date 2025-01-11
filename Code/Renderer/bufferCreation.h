#pragma once

#include <glm/fwd.hpp>

#include "VulkanIncludes/forward-declarations-renderer.h"

//Forward declaration
struct CommandPoolManager;
struct Vertex;

namespace BufferUtilities
{
    void createDeviceBuffer(VmaAllocator allocator, VkDeviceSize size, VkBufferUsageFlags usage,
                            VkDevice device, VmaAllocation* allocation, VkBuffer& buffer);
    void CreateImage(
        VkImageCreateInfo* pImageCreateInfo,
        VkImage* pImage, VmaAllocation* pAllocation, VkDevice device, VmaAllocator allocator, const char* debugName = "Bufferutiltiies create image vkimage");

    //TODO JS: move to cpp file?
    void stageMeshDataBuffer(VmaAllocator allocator, CommandPoolManager* commandPoolManager,
                             VkDeviceSize bufferSize, VkBuffer& buffer, VmaAllocation& allocation, void* vertices, VkBufferUsageFlags dataTypeFlag);

    void copyBuffer(CommandPoolManager* commandPoolManager, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
    
    void* createDynamicBuffer(VmaAllocator allocator, VkDeviceSize size,
                              VkBufferUsageFlags usage, VmaAllocation* allocation, VkBuffer& buffer);
    void createStagingBuffer(VkDeviceSize size,
                                   VmaAllocation* allocation, VkBuffer& stagingBuffer,  VkDevice device, VmaAllocator allocator, const char* debugName = "Bufferutiltiies create staging buffer buffer");
   
}
