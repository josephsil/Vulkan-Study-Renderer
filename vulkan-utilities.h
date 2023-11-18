#pragma once
#include <vector>
#include "Vulkan_Includes.h"
#include "common-structs.h"
class Scene;
//Forward declaration
struct RendererHandles;
struct CommandPoolManager;
struct TextureData;


struct dataBuffer
{
    VkBuffer data;
    uint32_t size;

    VkDescriptorBufferInfo getBufferInfo()
    {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = data; //TODO: For loop over frames in flight
        bufferInfo.offset = 0;
        bufferInfo.range = size;
        return bufferInfo;
    }
};


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

        WriteDescriptorSetsBuilder(int length)
        {
            descriptorsets.resize(length);
        }

        void Add(VkDescriptorType type, void* ptr, int count = 1)
        {
            descriptorsets[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorsets[i].dstBinding = i;
            descriptorsets[i].descriptorCount = count;
            descriptorsets[i].descriptorType = type;
            if (type == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE || type == VK_DESCRIPTOR_TYPE_SAMPLER)
            {
                descriptorsets[i].pImageInfo = static_cast<VkDescriptorImageInfo*>(ptr);
            }
            else
            {
                descriptorsets[i].pBufferInfo = static_cast<VkDescriptorBufferInfo*>(ptr);
            }
            i++;
        }

        void AddStorageBuffer(dataBuffer storageBuffer)
        {
            auto bufferInfo = storageBuffer.getBufferInfo();

            Add(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferInfo);
        }

        VkWriteDescriptorSet* data()
        {
            return descriptorsets.data();
        }

        uint32_t size()
        {
            return descriptorsets.size();
        }
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
    VkImageView createImageView(VkDevice device, VkImage image,
                                VkFormat format, VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT,
                                VkImageViewType type = VK_IMAGE_VIEW_TYPE_2D, uint32_t miplevels = 1,
                                uint32_t layerCount = 1);

    void createImage(RendererHandles rendererHandles, uint32_t width, uint32_t height, VkFormat format,
                     VkImageTiling tiling,
                     VkFlags usage, VkFlags properties, VkImage& image,
                     VkDeviceMemory& imageMemory, uint32_t miplevels = 1);

    void transitionImageLayout(RendererHandles rendererHandles, VkImage image, VkFormat format, VkImageLayout oldLayout,
                               VkImageLayout newLayout, VkCommandBuffer workingBuffer,
                               uint32_t miplevels = 1);

    void generateMipmaps(RendererHandles rendererHandles, VkImage image, VkFormat imageFormat, int32_t texWidth,
                         int32_t texHeight, uint32_t mipLevels);
    void copyBufferToImage(CommandPoolManager* commandPoolManager, VkBuffer buffer, VkImage image, uint32_t width,
                           uint32_t height,
                           VkCommandBuffer workingBuffer = nullptr);
}


//TODO JS: may be better to bundle stuff like createUniformBuffers, and the buffers themselves, along with these fns ala commandpoolmanager
namespace BufferUtilities
{
    void copyBuffer(RendererHandles rendererHandles, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
    void createBuffer(RendererHandles rendererHandles, VkDeviceSize size, VkBufferUsageFlags usage,
                      VkMemoryPropertyFlags properties, VkBuffer& buffer,
                      VkDeviceMemory& bufferMemory);
}
    