#pragma once
#include "../VulkanIncludes/forward-declarations-renderer.h"
#include <General/GLM_IMPL.h>

struct pointerSize
{
    void* ptr;
    uint32_t size;
};



struct simpleMeshPassInfo
{
    uint32_t firstIndex;
    uint32_t ct;
    VkPipeline pipeline;
    std::span<uint32_t> sortedObjectIDs;
};

struct ComputeCullListInfo
{
    uint32_t firstDrawIndirectIndex;
    uint32_t drawCount; //always object count?
    glm::mat4 viewMatrix;
    glm::mat4 projMatrix; //not used?
    VkPipelineLayout layout;
    pointerSize pushConstantInfo;
};


struct semaphoreData
{
    std::span<VkSemaphore> semaphores;
    std::span<VkPipelineStageFlags> waitStages;
};

struct DepthBufferInfo
    //This is like general image info -- currently only using for depth buffer/etc but need to get away from TextureData.h
{
    VkFormat format;
    VkImage image;
    VkImageView view;
    VmaAllocation vmaAllocation;
};

struct DepthPyramidInfo
{
    VkFormat format;
    VkImage image;
    std::span<VkImageView> viewsForMips;
    VmaAllocation vmaAllocation;
    glm::vec2 depthSize;
};

struct acquireImageSemaphore
{
    VkSemaphore semaphore{};
};






struct ActiveRenderStepData
{
    bool active;
    VkBuffer indexBuffer;
    VkPipeline boundPipeline;
    VkCommandBuffer commandBuffer;
    VkQueue Queue; 
    std::span<VkSemaphore> waitSemaphores;
    std::span<VkSemaphore> signalSempahores;
    std::span<VkDescriptorSet> boundDescriptorSets;
    VkFence* fence;
};
