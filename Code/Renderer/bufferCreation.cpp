#include "bufferCreation.h"


#include <atldef.h>
#include <cstdio>
#include <cstring>

#include "VulkanIncludes/VulkanMemory.h"
#include "VulkanIncludes/vmaImplementation.h"
#include "CommandPoolManager.h"
#include "rendererGlobals.h"

void createBuffer(VmaAllocator allocator, VkDeviceSize size, VkBufferUsageFlags usage, VmaAllocationCreateFlags  flags,
                  VmaAllocation* allocation, VkBuffer* buffer, VmaAllocationInfo* outAllocInfo)
{

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO; //TODO JS: Pass in Properties flags?
    allocInfo.flags = flags;
    vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, buffer, allocation, outAllocInfo);
}

void BufferUtilities::copyBuffer(CommandPoolManager* commandPoolManager, VkBuffer srcBuffer, VkBuffer dstBuffer,
                                 VkDeviceSize size)
{
    VkCommandBuffer commandBuffer =commandPoolManager->beginSingleTimeCommands_transfer();

    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

   commandPoolManager->endSingleTimeCommands(commandBuffer);
}


void BufferUtilities::stageMeshDataBuffer(VmaAllocator allocator, CommandPoolManager* commandPoolManager, VkDeviceSize bufferSize, VkBuffer& buffer,
                                   VmaAllocation& allocation, void* vertices, VkBufferUsageFlags dataTypeFlag)
{
    VkBuffer stagingBuffer;

    VmaAllocation stagingallocation = {};
  
    createBuffer(allocator, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
                                  &stagingallocation, &stagingBuffer, nullptr);


    void* data;
    VulkanMemory::MapMemory(allocator, stagingallocation,&data);
    memcpy(data, vertices, bufferSize);
    VulkanMemory::UnmapMemory(allocator, stagingallocation);

    createBuffer(allocator, bufferSize,
                                  VK_BUFFER_USAGE_TRANSFER_DST_BIT | dataTypeFlag, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT, &allocation, &buffer, nullptr);

    copyBuffer(commandPoolManager, stagingBuffer, buffer, bufferSize);

    //delete temp buffer
    vmaDestroyBuffer(allocator, stagingBuffer, stagingallocation);
    vmaFreeMemory(allocator, stagingallocation);

 
}

void* BufferUtilities::createDynamicBuffer(VmaAllocator allocator, VkDeviceSize size, VkBufferUsageFlags usage,
                                           VmaAllocation* allocation, VkBuffer& buffer)
{
    VmaAllocationInfo allocInfo;
    createBuffer(allocator, size, usage, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                 VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT |
                 VMA_ALLOCATION_CREATE_MAPPED_BIT,
                 allocation, &buffer, &allocInfo);


    VkMemoryPropertyFlags memPropFlags;
    vmaGetAllocationMemoryProperties(allocator, *allocation, &memPropFlags);

    if (memPropFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
    {
        return allocInfo.pMappedData;
    }
    else
    {
        printf("NOT IMPLEMENTED: Allocation ended up in unmapped memory, see https://gpuopen-librariesandsdks.github.io/VulkanMemoryAllocator/html/usage_patterns.html advanced data uploading");
    exit(-1);
    }
}


void BufferUtilities::createDeviceBuffer(VmaAllocator allocator, VkDeviceSize size, VkBufferUsageFlags usage,
                                           VkDevice device,VmaAllocation* allocation, VkBuffer& buffer)
{
    VmaAllocationInfo AllocationInfo = {};
    createBuffer(allocator, size, usage,  VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
                 allocation, &buffer, &AllocationInfo);
    setDebugObjectName(device, VK_OBJECT_TYPE_BUFFER, "Bufferutiltiies create device buffer buffer", (uint64_t)buffer);

}
void BufferUtilities::createStagingBuffer(VkDeviceSize size,
VmaAllocation* allocation, VkBuffer& stagingBuffer,  VkDevice device, VmaAllocator allocator, const char* debugName)
{
    createBuffer(allocator, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
   VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT, allocation,
                                 &stagingBuffer, nullptr);
    setDebugObjectName(device, VK_OBJECT_TYPE_BUFFER, "Bufferutiltiies create staging buffer buffer", (uint64_t)stagingBuffer);
    
}

void BufferUtilities::CreateImage(
    VkImageCreateInfo* pImageCreateInfo,
    VkImage* pImage,
    VmaAllocation* pAllocation,
    VkDevice device,
    VmaAllocator allocator,
    const char* debugName)
{
    
    VmaAllocationCreateInfo allocInfo = {};
    VmaAllocationInfo AllocationInfo = {};
    vmaCreateImage(allocator, pImageCreateInfo, &allocInfo, pImage, pAllocation, &AllocationInfo);
    setDebugObjectName(device, VK_OBJECT_TYPE_IMAGE, debugName, (uint64_t)*pImage);
}


