#pragma once
#include <Renderer/VulkanIncludes/forward-declarations-renderer.h>
#include <General/GLM_impl.h>

#include "PipelineManager/PipelineLayoutManager.h"

struct pointerSize
{
    void* ptr;
    uint32_t size;
};


//TODO JS MESHLET PERF
//Need to replace these: in the future, I want meshlets to share a single vertex buffer, and this should point to the vertex buffer and give info (as below) to navigate the index buffers
//Each mesh has a span of these, which says how to index in to its submeshes meshlets
struct meshletIndexInfo
{
    size_t offset;
    size_t ct;
};

struct drawCommandsPassOffset
{
    uint32_t firstDrawIndex;
};

struct simpleMeshPassInfo
{
    drawCommandsPassOffset offset;
    uint32_t drawCount;
    FullShaderHandle shader;
    
    //Both of these are indexed into BY NUMBER OF SUBMESHES IN BATCH
    std::span<uint32_t> sortedSubmeshes;
    std::span<uint32_t>  sortedfirstIndices; 
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

struct imageSize
{
    uint32_t x;
    uint32_t y;
};
struct DepthPyramidInfo
{
    VkFormat format;
    VkImage image;
    VkImageView view;
    std::span<VkImageView> viewsForMips;
    VmaAllocation vmaAllocation;
    imageSize depthSize;
};

struct FrameSemaphores
{
    VkSemaphore swapchainSemaphore{};
    VkSemaphore prepassSemaphore{};
    VkSemaphore opaqueSemaphore{};
    VkSemaphore presentSemaphore{};
    VkSemaphore cullingSemaphore{};
    VkFence cullingFence{};
};






struct ActiveRenderStepData
{
    bool commandBufferActive;
    VkBuffer boundIndexBuffer;
    VkPipeline boundPipeline;
    VkCommandBuffer commandBuffer;
    VkQueue Queue; 
    VkSemaphore* waitSemaphore;
    uint32_t waitSemaphoreCt;
    VkSemaphore* signalSempahore;
    uint32_t signalSemaphoreCt;
    std::span<VkDescriptorSet> boundDescriptorSets;
    VkFence* fence;
};
