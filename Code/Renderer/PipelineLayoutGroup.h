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


struct DescriptorSetRefs
{
    VkDescriptorSetLayout layout;
    std::span<VkDescriptorSet> perFrameSets;
};

//TODO JS: Pipelines identified by slots? Unique perPipelineData based on configuration???
//
    //One of these per pipeline *layout*, contains multiple pipelines
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
        PipelineLayoutGroup(RendererContext handles, VkDescriptorPool pool, std::span<DescriptorSetRefs> RequiredGeneralDescriptorInfo, std::span<VkDescriptorSetLayoutBinding> PerPipelineDescriptorSetLayout,  uint32_t perDrawSetsPerFrame, const char* debugName);

        //Initialization
        

        void createGraphicsPipeline(std::span<VkPipelineShaderStageCreateInfo> shaders, char* name, graphicsPipelineSettings settings, size_t pconstantSize
                                        = 0);
        void createComputePipeline(VkPipelineShaderStageCreateInfo shader, char* name, size_t pconstantSize = 0);
        struct perPipelineData;

        //Runtime
        void UpdatePerPipelineDescriptorSets(RendererContext rendererHandles, std::span<descriptorUpdateData> descriptorUpdates, uint32_t currentFrame, uint32_t
                                             setToUpdate);
        void BindRequiredDescriptorSetsToCommandBuffer(VkCommandBuffer cmd, std::span<VkDescriptorSet> currentlyBoundSets, uint32_t currentFrame, uint32_t descriptorOffset, VkPipelineBindPoint
                                 bindPoint =
                                     VK_PIPELINE_BIND_POINT_GRAPHICS);

        VkPipeline getPipeline(int index);
        uint32_t getPipelineCt();
        void cleanup();

  
        struct perPipelineData
        {
            std::span<VkDescriptorSetLayoutBinding> slots;

            bool pipelineLayoutInitialized = false;
            bool pipelinesInitialized = false;
            bool descriptorSetsInitialized = false;

            
            VkPipelineLayout PipelineLayout;
            std::span<std::span<VkDescriptorSet>> RequiredGeneralPurposeDescriptorSets;
            std::vector<VkDescriptorSetLayout> ExternalDescriptorSetLayouts = {};
            VkDescriptorSetLayout perShaderDescriptorSetLayout = {};

        
            std::vector<std::span<VkDescriptorSet>> PerLayoutDescriptorSets = {};


            void cleanup(VkDevice device);
        };

        perPipelineData pipelineData;
        VkPipelineLayoutCreateInfo partialPipelinelayoutCreateInfo; //Has the layouts, but not push constants
        // perPipelineData shadowPipelineData;
        //Descriptor set update
        void initializePerShaderDescriptorSets(RendererContext handles, VkDescriptorPool pool, int MAX_FRAMES_IN_FLIGHT, uint32_t descriptorCt, const char* debugName);
        VkPipeline GetPipelineByName(const char* name);

    private:

       
        
        VkDevice device;

      
        std::vector<VkPipeline> pipelines;

        void createLayout(RendererContext handles,  std::span<
                          VkDescriptorSetLayoutBinding> PerPipelineDescriptorSetLayout, const char* debugName);


     
        void createPipelineLayoutForPipeline(perPipelineData* perPipelineData, size_t pconstantSize, char* name, bool
                                             compute);
        void UpdatePerPipelineDescriptorSets(std::span<descriptorUpdateData> descriptorUpdates, uint32_t currentFrame, perPipelineData* perPipelineData, uint32_t
                                             descriptorOffset);
        std::vector<VkWriteDescriptorSet> writeDescriptorSets;
        std::unordered_map<VkDescriptorSet, int> writeDescriptorSetsBindingIndices;
        
    };