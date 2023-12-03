#include "bufferCreation.h"

#include "Memory.h"
#include "vmaImplementation.h"
#include "AppStruct.h"
#include "CommandPoolManager.h"

void createBuffer(RendererHandles rendererHandles, VkDeviceSize size, VkBufferUsageFlags usage, VmaAllocationCreateFlags  flags,
                                   VmaAllocation* allocation, VkBuffer& buffer, VmaAllocationInfo* outAllocInfo)
{

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocator allocator = rendererHandles.allocator;
    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO; //TODO JS: Pass in Properties flags?
    allocInfo.flags = flags;

    vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &buffer, allocation, outAllocInfo);
}

void BufferUtilities::copyBuffer(RendererHandles rendererHandles, VkBuffer srcBuffer, VkBuffer dstBuffer,
                                 VkDeviceSize size)
{
    VkCommandBuffer commandBuffer = rendererHandles.commandPoolmanager->beginSingleTimeCommands_transfer();

    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    rendererHandles.commandPoolmanager->endSingleTimeCommands(commandBuffer);
}

void BufferUtilities::stageVertexBuffer(RendererHandles rendererHandles, VkDeviceSize bufferSize, VkBuffer& buffer,
                                   VmaAllocation& allocation, Vertex* data)
{
    BufferUtilities::stageMeshDataBuffer(rendererHandles,
                                         bufferSize,
                                         buffer,
                                         allocation,
                                         data, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    
}

void BufferUtilities::stageIndexBuffer(RendererHandles rendererHandles, VkDeviceSize bufferSize, VkBuffer& buffer,
                                   VmaAllocation& allocation, uint32_t* data)
{
    BufferUtilities::stageMeshDataBuffer(rendererHandles,
                                         bufferSize,
                                         buffer,
                                         allocation,
                                         data, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    
}

void BufferUtilities::stageMeshDataBuffer(RendererHandles rendererHandles, VkDeviceSize bufferSize, VkBuffer& buffer,
                                   VmaAllocation& allocation, void* vertices, VkBufferUsageFlags dataTypeFlag)
{
    VkBuffer stagingBuffer;

    VmaAllocation stagingallocation = {};
  
    createBuffer(rendererHandles, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
                                  &stagingallocation, stagingBuffer, nullptr);


    void* data;
    VulkanMemory::MapMemory(rendererHandles.allocator, stagingallocation,&data);
    memcpy(data, vertices, bufferSize);
    VulkanMemory::UnmapMemory(rendererHandles.allocator, stagingallocation);

    createBuffer(rendererHandles, bufferSize,
                                  VK_BUFFER_USAGE_TRANSFER_DST_BIT | dataTypeFlag, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT, &allocation, buffer, nullptr);

    copyBuffer(rendererHandles, stagingBuffer, buffer, bufferSize);

    vmaDestroyBuffer(rendererHandles.allocator, stagingBuffer, stagingallocation);
}

void* BufferUtilities::createDynamicBuffer(RendererHandles rendererHandles, VkDeviceSize size, VkBufferUsageFlags usage,
                                           VmaAllocation* allocation, VkBuffer& buffer)
{
    VmaAllocationInfo allocInfo;
    createBuffer(rendererHandles, size, usage, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                 VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT |
                 VMA_ALLOCATION_CREATE_MAPPED_BIT,
                 allocation, buffer, &allocInfo);


    VkMemoryPropertyFlags memPropFlags;
    vmaGetAllocationMemoryProperties(rendererHandles.allocator, *allocation, &memPropFlags);

    if (memPropFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
    {
        return allocInfo.pMappedData;
    }
    else
    {
        printf("NOT IMPLEMENTED: Allocation ended up in unmapped memory, see https://gpuopen-librariesandsdks.github.io/VulkanMemoryAllocator/html/usage_patterns.html advanced data uploading");
        ;
    }
}
void BufferUtilities::createStagingBuffer(RendererHandles rendererHandles, VkDeviceSize size,
                                   VmaAllocation* allocation, VkBuffer& stagingBuffer)
{
    createBuffer(rendererHandles, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
   VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT, allocation,
                                 stagingBuffer, nullptr);
    
}

void BufferUtilities::CreateImage( VmaAllocator allocator,
    VkImageCreateInfo* pImageCreateInfo,
    VkImage* pImage,
    VmaAllocation* pAllocation,
    VkDeviceMemory* deviceMemory)
{
    
    VmaAllocationCreateInfo allocInfo = {};
    VmaAllocationInfo AllocationInfo;
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    
    vmaCreateImage(allocator, pImageCreateInfo, &allocInfo, pImage, pAllocation, &AllocationInfo);
    deviceMemory = &AllocationInfo.deviceMemory;
}


