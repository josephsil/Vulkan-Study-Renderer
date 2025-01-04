#pragma once

#include <glm/fwd.hpp>

#include "VulkanIncludes/forward-declarations-renderer.h"

struct Vertex;
class RendererLoadedAssetData;
//Forward declaration
struct RendererContext;
struct CommandPoolManager;
struct TextureData;
struct Vertex;

namespace BufferUtilities
{
    void CreateImage(
        RendererContext handles,
        VkImageCreateInfo* pImageCreateInfo,
        VkImage* pImage, VmaAllocation* pAllocation, VkDeviceMemory* deviceMemory);
    
    void stageVertexBufferAndRegisterDeletion(RendererContext rendererHandles, VkDeviceSize bufferSize, VkBuffer& buffer,
                           VmaAllocation& allocation, Vertex* data);

    void stageIndexBufferAndRegisterDeletion(RendererContext rendererHandles, VkDeviceSize bufferSize, VkBuffer& buffer,
                          VmaAllocation& allocation, glm::uint32_t* data);

    //TODO JS: move to cpp file?
    void stageMeshDataBuffer(RendererContext rendererHandles, VkDeviceSize bufferSize, VkBuffer& buffer,
                             VmaAllocation& allocation, void* vertices, VkBufferUsageFlags dataTypeFlag);

    void copyBuffer(RendererContext rendererHandles, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
    
    void* createDynamicBuffer(RendererContext rendererHandles, VkDeviceSize size, VkBufferUsageFlags usage,
                              VmaAllocation* allocation, VkBuffer& buffer);
    void createStagingBuffer(RendererContext rendererHandles, VkDeviceSize size,
                                   VmaAllocation* allocation, VkBuffer& stagingBuffer);
   
}
