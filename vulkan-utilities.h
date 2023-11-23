#pragma once
#include <vector>


#include "vulkan-forwards.h"
#include "common-structs.h"
struct Vertex;
class Scene;
//Forward declaration
struct RendererHandles;
struct CommandPoolManager;
struct TextureData;


struct dataBuffer
{
    VkBuffer data;
    uint32_t size;

    VkDescriptorBufferInfo getBufferInfo();
};


void DestroyBuffer(VkDevice device, VkBuffer buffer);
void FreeMemory(VkDevice device, VkDeviceMemory memory);
void UnmapMemory(VkDevice device, VkDeviceMemory memory);


void MapMemory(VkDevice device, VkDeviceMemory memory, VkDeviceSize size, void** data);

namespace DescriptorDataUtilities
{
    std::pair<std::vector<VkDescriptorImageInfo>, std::vector<VkDescriptorImageInfo>> ImageInfoFromImageDataVec(
        std::vector<TextureData> textures);
    //TODO JS: Move to textureData? 
    std::pair<VkDescriptorImageInfo, VkDescriptorImageInfo> ImageInfoFromImageData(TextureData texture);

    //TODO JS: move
    struct WriteDescriptorSetsBuilder
    {
        std::vector<VkWriteDescriptorSet> descriptorsets;
        int i = 0;

        WriteDescriptorSetsBuilder(int length);

        void Add(VkDescriptorType type, void* ptr, int count = 1);
      

        void AddStorageBuffer(dataBuffer storageBuffer);

        VkWriteDescriptorSet* data();

        uint32_t size();
    };
}

namespace DescriptorSetSetup
{
    void createBindlessLayout(RendererHandles rendererHandles, Scene* scene, VkDescriptorSetLayout* layout);
}

namespace RenderingSetup
{
    void createRenderPass(RendererHandles rendererHandles, RenderTextureFormat passformat, VkRenderPass* pass);
}

namespace Capabilities
{
    VkFormat findDepthFormat(RendererHandles rendererHandles);

    VkFormat findSupportedFormat(RendererHandles rendererHandles, const std::vector<VkFormat>& candidates,
                                 VkImageTiling tiling,
                                 VkFormatFeatureFlags features);

    uint32_t findMemoryType(RendererHandles rendererHandles, uint32_t typeFilter, VkMemoryPropertyFlags properties);
}

namespace TextureUtilities
{
    //TODO JS: this -1 stuff is sketchy
    VkImageView createImageView(VkDevice device, VkImage image,
                                VkFormat format, VkImageAspectFlags aspectFlags = -1,
                                VkImageViewType type = (VkImageViewType)-1, uint32_t miplevels = 1,
                                uint32_t layerCount = 1);

    void createImage(RendererHandles rendererHandles, uint32_t width, uint32_t height, VkFormat format,
                     VkImageTiling tiling,
                     VkFlags usage, VkFlags properties, VkImage& image,
                     VmaAllocation& allocation, uint32_t miplevels = 1);

    void transitionImageLayout(RendererHandles rendererHandles, VkImage image, VkFormat format, VkImageLayout oldLayout,
                               VkImageLayout newLayout, VkCommandBuffer workingBuffer,
                               uint32_t miplevels = 1);

    void generateMipmaps(RendererHandles rendererHandles, VkImage image, VkFormat imageFormat, int32_t texWidth,
                         int32_t texHeight, uint32_t mipLevels);
    void copyBufferToImage(CommandPoolManager* commandPoolManager, VkBuffer buffer, VkImage image, uint32_t width,
                           uint32_t height,
                           VkCommandBuffer workingBuffer = nullptr);
}



namespace BufferUtilities
{
    void stageVertexBuffer(RendererHandles rendererHandles, VkDeviceSize bufferSize, VkBuffer& buffer,
                                   VkDeviceMemory& bufferMemory, Vertex* data);

    void stageIndexBuffer(RendererHandles rendererHandles, VkDeviceSize bufferSize, VkBuffer& buffer,
                                  VkDeviceMemory& bufferMemory, uint32_t* data);

    //TODO JS: move to cpp file?
    void stageMeshDataBuffer(RendererHandles rendererHandles, VkDeviceSize bufferSize, VkBuffer& buffer,
                                   VkDeviceMemory& bufferMemory, void* vertices, VkBufferUsageFlags dataTypeFlag);

    void copyBuffer(RendererHandles rendererHandles, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
    void* createDynamicBuffer(RendererHandles rendererHandles, VkDeviceSize size, VkBufferUsageFlags usage,
                              VmaAllocation* allocation, VkBuffer& buffer);
    void createBuffer(RendererHandles rendererHandles, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags flags,
                      VmaAllocation* allocation, VkBuffer& buffer, VmaAllocationInfo* allocInfo);
}
    