#pragma once

#include <span>
#include <unordered_map>
#include <vector>
#include "Renderer/VulkanIncludes/Vulkan_Includes.h"
struct Vertex;
//Forward declaration
struct RendererContext;
struct CommandPoolManager;
struct descriptorUpdateData;

//can have one or more descriptor sets to draw per frame. Most only have one
//Need multiple for cases where we need to "update" more than once per frame, for non-bindless style drawing
//TODO should remove this concept and dynamically grab them them from the pool as required
typedef std::span<VkDescriptorSet> descriptorSetsForGroup;
typedef std::span<descriptorUpdateData> descriptorUpdates;

struct DescriptorDataForPipeline
{
    bool isPerFrame;
    descriptorSetsForGroup* descriptorSetsCache; //Either an array of size MAX_FRAMES_IN_FLIGHT or 1 descriptorSetsForGroup depending on isPerFrame
    std::span<VkDescriptorSetLayoutBinding> layoutBindings;
};


    class PipelineLayoutGroup
    {
    public:

        struct graphicsPipelineSettings
        {
            std::span<VkFormat> colorFormats;
            VkFormat depthFormat;
            VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT;
            VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            VkBool32 depthClampEnable = VK_FALSE;
            VkBool32 depthWriteEnable = VK_TRUE;
            VkBool32 depthBias = VK_FALSE;
            bool dynamicBias = false;
        };

        
        PipelineLayoutGroup()
        {}
        PipelineLayoutGroup(RendererContext handles, VkDescriptorPool pool,
   std::span<DescriptorDataForPipeline> descriptorInfo, std::span<VkDescriptorSetLayout> layouts, uint32_t perDrawSetsPerFrame, uint32_t pconstantsize, bool compute,
    const char* debugName);

        //Initialization
        
        void createPipeline(std::span<VkPipelineShaderStageCreateInfo> shaders, const char* name, graphicsPipelineSettings settings);
        void createGraphicsPipeline(std::span<VkPipelineShaderStageCreateInfo> shaders, char* name, graphicsPipelineSettings settings);
        void createComputePipeline(VkPipelineShaderStageCreateInfo shader, char* name, size_t pconstantSize = 0);
        struct perPipelineData;

        //Runtime
        void BindRequiredDescriptorSetsToCommandBuffer(VkCommandBuffer cmd, std::span<VkDescriptorSet> currentlyBoundSets, uint32_t currentFrame, uint32_t descriptorIndex, VkPipelineBindPoint
                                 bindPoint =VK_PIPELINE_BIND_POINT_GRAPHICS);

        VkPipeline getPipeline(int index);
        uint32_t getPipelineCt();
        void cleanup();

  
        struct perPipelineData
        {
            bool iscompute;
            VkPipelineLayout layout;
            std::span<DescriptorDataForPipeline> descriptorConfiguration;
            void cleanup(VkDevice device);
        };

        perPipelineData pipelineData;
        VkPipelineLayoutCreateInfo partialPipelinelayoutCreateInfo; //Has the layouts, but not push constants
        // perPipelineData shadowPipelineData;
        //Descriptor set update

    private:
        VkDevice device;
        std::vector<VkPipeline> pipelines;
        void createPipelineLayoutForGroup(perPipelineData* perPipelineData, size_t pconstantSize, char* name, bool
                                             compute);
        std::vector<VkWriteDescriptorSet> writeDescriptorSets;
        std::unordered_map<VkDescriptorSet, int> writeDescriptorSetsBindingIndices;
        
    };