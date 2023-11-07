#pragma once


#include <vector>
#include <vulkan/vulkan_core.h>


#include "Vulkan_Includes.h"
//Forward declaration
class HelloTriangleApplication;
struct VkPipelineShaderStageCreateInfo;
struct VkPipelineShaderStageCreateInfo;
typedef struct VkPhysicalDevice_T* VkPhysicalDevice;
typedef struct VkDevice_T* VkDevice;
typedef struct VkDescriptorSetLayout_T* VkDescriptorSetLayout;
typedef struct VkBuffer_T* VkBuffer;
typedef struct VkDeviceMemory_T* VkDeviceMemory;
typedef struct VkImageView_T* VkImageView;
typedef struct VkRenderPass_T* VkRenderPass;
typedef struct VkSampler_T* VkSampler;
struct CommandPoolManager;
enum VkFormat;

namespace DescriptorSetSetup 
{
    void createBindlessLayout(HelloTriangleApplication* app,  VkDescriptorSetLayout* layout);
}

namespace RenderingSetup 
{
    
    void createRenderPass(HelloTriangleApplication* app, RenderTextureFormat passformat, VkRenderPass* pass);

}

namespace Capabilities
{
    VkFormat findDepthFormat(HelloTriangleApplication* app);
    
    VkFormat findSupportedFormat(HelloTriangleApplication* app, const std::vector<VkFormat>& candidates, VkImageTiling tiling,
                                                           VkFormatFeatureFlags features);

    uint32_t findMemoryType(HelloTriangleApplication* app, uint32_t typeFilter, VkMemoryPropertyFlags properties);
}

namespace TextureUtilities
{
    VkImageView createImageView(VkDevice device, VkImage image,
        VkFormat format, VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT,
        VkImageViewType type = VK_IMAGE_VIEW_TYPE_2D, uint32_t miplevels = 1, uint32_t layerCount = 1);

    void createImage(HelloTriangleApplication* app, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling,
                                           VkFlags usage, VkFlags properties, VkImage& image,
                                           VkDeviceMemory& imageMemory, uint32_t miplevels = 1);

    void transitionImageLayout(HelloTriangleApplication* app, VkImage image, VkFormat format, VkImageLayout oldLayout,
                                                     VkImageLayout newLayout, VkCommandBuffer workingBuffer,
                                                     uint32_t miplevels = 1);

    void generateMipmaps(HelloTriangleApplication* app, VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels);
    void copyBufferToImage(CommandPoolManager* commandPoolManager, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height,
                              VkCommandBuffer workingBuffer = nullptr);

}


//TODO JS: may be better to bundle stuff like createUniformBuffers, and the buffers themselves, along with these fns ala commandpoolmanager
namespace  BufferUtilities
{

    void copyBuffer(HelloTriangleApplication* app, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
    void createBuffer(HelloTriangleApplication* app, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer,
                      VkDeviceMemory& bufferMemory);
    
}
