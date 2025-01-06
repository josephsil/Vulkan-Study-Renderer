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



//TODO JS: Pipelines identified by slots? Unique perPipelineData based on configuration???
//
    //One of these per pipeline *layout*, contains multiple pipelines
    class PipelineDataObject
    {
    public:

        struct graphicsPipelineSettings
        {
            std::span<VkFormat> colorFormats;
            VkFormat depthFormat;
            VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT;
            VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            VkBool32 depthBias = VK_FALSE;
            bool dynamicBias = false;
        };

        
        PipelineDataObject()
        {}
        PipelineDataObject(RendererContext handles, VkDescriptorPool pool, std::span<VkDescriptorSetLayoutBinding> opaqueLayout);

        //Initialization
        

        void createGraphicsPipeline(std::vector<VkPipelineShaderStageCreateInfo> shaders, graphicsPipelineSettings settings, size_t pconstantSize = 0);
        void createComputePipeline(VkPipelineShaderStageCreateInfo shader, size_t pconstantSize = 0);
        struct perPipelineData;

        //Runtime
        void updateDescriptorSets(std::span<descriptorUpdateData> descriptorUpdates, uint32_t currentFrame);
        void bindToCommandBuffer(VkCommandBuffer cmd, uint32_t currentFrame, VkPipelineBindPoint bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS);

        VkPipeline getPipeline(int index);
        uint32_t getPipelineCt();
        void cleanup();

      

        struct perPipelineData
        {
            std::span<VkDescriptorSetLayoutBinding> slots;

            bool pipelineLayoutInitialized = false;
            bool pipelinesInitialized = false;
            bool descriptorSetsInitialized = false;

            
            VkPipelineLayout bindlessPipelineLayout;
            VkDescriptorSetLayout perSceneDescriptorSetLayout = {};
            // VkDescriptorSetLayout storageDescriptorLayout = {};
            // VkDescriptorSetLayout imageDescriptorLayout = {};
            // VkDescriptorSetLayout samplerDescriptorLayout = {};
        
            std::vector<VkDescriptorSet> perSceneDescriptorSetForFrame = {};
            // std::vector<VkDescriptorSet> storageDescriptorSetForFrame = {};
            // std::vector<VkDescriptorSet> imageDescriptorSetForFrame = {};
            // std::vector<VkDescriptorSet> samplerDescriptorSetForFrame = {};

            VkDescriptorSet getSetFromType(VkDescriptorType type, int currentFrame);
            void cleanup(VkDevice device);
        };

        perPipelineData pipelineData;
        // perPipelineData shadowPipelineData;
        //Descriptor set update
        void createDescriptorSets(VkDescriptorPool pool, int MAX_FRAMES_IN_FLIGHT);

    private:

       
        
        VkDevice device;
     

      
        std::vector<VkPipeline> pipelines;

            //TODO JS: System to share these for multiple pipelines. these are really per LAYOUT data
    

 

        void createLayout(RendererContext handles, std::span<VkDescriptorSetLayoutBinding> layout);


     
        void createPipelineLayoutForPipeline(perPipelineData* perPipelineData, size_t pconstantSize, bool compute);
        void updateDescriptorSetsForPipeline(std::span<descriptorUpdateData> descriptorUpdates, uint32_t currentFrame, perPipelineData* perPipelineData);
        std::vector<VkWriteDescriptorSet> writeDescriptorSets;
        std::unordered_map<VkDescriptorSet, int> writeDescriptorSetsBindingIndices;
        
    };