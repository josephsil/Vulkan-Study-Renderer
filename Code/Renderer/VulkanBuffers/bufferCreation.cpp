#include "bufferCreation.h"


#include <atldef.h>
#include <cstdio>
#include <cstring>

#include <Renderer/VulkanIncludes/VulkanMemory.h>
#include <Renderer/VulkanIncludes/vmaImplementation.h>
#include <Renderer/CommandPoolManager.h>
#include <Renderer/rendererGlobals.h>


void CreateBuffer(VmaAllocator allocator, VkDeviceSize size, VkBufferUsageFlags usage, VmaAllocationCreateFlags flags,
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

void BufferUtilities::CopyBuffer(VkCommandBuffer cb, VkBuffer srcBuffer, VkBuffer dstBuffer,
                                                  VkDeviceSize size)
{
    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(cb, srcBuffer, dstBuffer, 1, &copyRegion);
}


void BufferUtilities::CreateDeviceBuffer(VmaAllocator allocator, VkDeviceSize size, VkBufferUsageFlags usage,
                                         VmaAllocation* allocation, VkBuffer& buffer)
{
    VmaAllocationInfo allocInfo;
    CreateBuffer(allocator, size, usage, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
                 allocation, &buffer, &allocInfo);


    VkMemoryPropertyFlags memPropFlags;
    vmaGetAllocationMemoryProperties(allocator, *allocation, &memPropFlags);
}

void* BufferUtilities::CreateHostMappedBuffer(VmaAllocator allocator, VkDeviceSize size, VkBufferUsageFlags usage,
                                              VmaAllocation* allocation, VkBuffer& buffer)
{
    VmaAllocationInfo allocInfo;
    CreateBuffer(allocator, size, usage, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                 VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT |
                 VMA_ALLOCATION_CREATE_MAPPED_BIT,
                 allocation, &buffer, &allocInfo);


    VkMemoryPropertyFlags memPropFlags;
    vmaGetAllocationMemoryProperties(allocator, *allocation, &memPropFlags);

    if (memPropFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
    {
        return allocInfo.pMappedData;
    }
    printf(
        "NOT IMPLEMENTED: Allocation ended up in unmapped memory, see https://gpuopen-librariesandsdks.github.io/VulkanMemoryAllocator/html/usage_patterns.html advanced data uploading");
    exit(-1);
}


void BufferUtilities::CreateDeviceBuffer(VmaAllocator allocator, const char* name, VkDeviceSize size, VkBufferUsageFlags usage,
                                         VkDevice device, VmaAllocation* allocation, VkBuffer* buffer)
{
    VmaAllocationInfo AllocationInfo = {};
    CreateBuffer(allocator, size, usage, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                 allocation, buffer, &AllocationInfo);
    SetDebugObjectNameS(device, VK_OBJECT_TYPE_BUFFER, name, (uint64_t)*buffer);
}

void BufferUtilities::CreateStagingBuffer(VkDeviceSize size,
                                          VmaAllocation* allocation, VkBuffer& stagingBuffer, VkDevice device,
                                          VmaAllocator allocator, const char* debugName)
{
    CreateBuffer(allocator, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT, allocation,
                 &stagingBuffer, nullptr);
    SetDebugObjectNameS(device, VK_OBJECT_TYPE_BUFFER, "Bufferutiltiies create staging buffer buffer",
                       (uint64_t)stagingBuffer);
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
    SetDebugObjectNameS(device, VK_OBJECT_TYPE_IMAGE, debugName, (uint64_t)*pImage);
}
