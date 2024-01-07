#pragma once

#include <glm/fwd.hpp>

#include "VulkanIncludes/forward-declarations-renderer.h"

struct Vertex;
class Scene;
//Forward declaration
struct RendererHandles;
struct CommandPoolManager;
struct TextureData;
struct Vertex;

namespace BufferUtilities
{
    void CreateImage(VmaAllocator allocator,
                     VkImageCreateInfo* pImageCreateInfo,
                     VkImage* pImage,
                     VmaAllocation* pAllocation, VkDeviceMemory* deviceMemory);
    
    void stageVertexBuffer(RendererHandles rendererHandles, VkDeviceSize bufferSize, VkBuffer& buffer,
                           VmaAllocation& allocation, Vertex* data);

    void stageIndexBuffer(RendererHandles rendererHandles, VkDeviceSize bufferSize, VkBuffer& buffer,
                          VmaAllocation& allocation, glm::uint32_t* data);

    //TODO JS: move to cpp file?
    void stageMeshDataBuffer(RendererHandles rendererHandles, VkDeviceSize bufferSize, VkBuffer& buffer,
                             VmaAllocation& allocation, void* vertices, VkBufferUsageFlags dataTypeFlag);

    void copyBuffer(RendererHandles rendererHandles, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
    
    void* createDynamicBuffer(RendererHandles rendererHandles, VkDeviceSize size, VkBufferUsageFlags usage,
                              VmaAllocation* allocation, VkBuffer& buffer);
    void createStagingBuffer(RendererHandles rendererHandles, VkDeviceSize size,
                                   VmaAllocation* allocation, VkBuffer& stagingBuffer);
   
}
