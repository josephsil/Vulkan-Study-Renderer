#pragma once

#include <span>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan_core.h>

#include "RendererHandles.h"
#include "VulkanIncludes/forward-declarations-renderer.h"

struct Vertex;
//Forward declaration
struct RendererHandles;
struct CommandPoolManager;
struct TextureData;
struct descriptorUpdateData;



//TODO JS: Pipelines identified by slots? Unique perPipelineData based on configuration???
//
 //Sorta material-esque -- should this grow, and own information about the corresponding pipeline?
    class PipelineDataObject
    {
    public:
        PipelineDataObject()
        {}
        PipelineDataObject(RendererHandles handles, std::span<VkDescriptorSetLayoutBinding> opaqueLayout, std::span<VkDescriptorSetLayoutBinding> shadowlayout);

        //Initialization
       
        void createGraphicsPipeline(std::vector<VkPipelineShaderStageCreateInfo> shaders, VkFormat* swapchainFormat, VkFormat* depthFormat, bool shadow, bool color = true, bool
                                    depth = true, bool lines = false);

        //Runtime
        void updateOpaqueDescriptorSets(std::span<descriptorUpdateData> descriptorUpdates, uint32_t currentFrame);
        void updateShadowDescriptorSets(std::span<descriptorUpdateData> descriptorUpdates, uint32_t currentFrame);
        void createDescriptorSetsOpaque(VkDescriptorPool pool, int MAX_FRAMES_IN_FLIGHT);
        void createDescriptorSetsShadow(VkDescriptorPool pool, int MAX_FRAMES_IN_FLIGHT);
        void bindToCommandBufferOpaque(VkCommandBuffer cmd, uint32_t currentFrame);
        void bindToCommandBufferShadow(VkCommandBuffer cmd, uint32_t currentFrame);

        VkPipeline getPipeline(int index);
        uint32_t getPipelineCt();
        VkPipelineLayout getLayoutOpaque();
        VkPipelineLayout getLayoutShadow();
        void createPipelineLayoutShadow();
        void createPipelineLayoutOpaque();

        void cleanup();

    private:

       
        
        VkDevice device;

      
        std::vector<VkPipeline> bindlesspipelineObjects;

            //TODO JS: System to share these for multiple pipelines. these are really per LAYOUT data
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

        perPipelineData opaquePipelineData;
        perPipelineData shadowPipelineData;

        void bindToCommandBuffer(VkCommandBuffer cmd, uint32_t currentFrame,perPipelineData* pipelinedata);
        void createOpaqueLayout(RendererHandles handles, std::span<VkDescriptorSetLayoutBinding> layout);
        void createShadowLayout(RendererHandles handles, std::span<VkDescriptorSetLayoutBinding> layout);


        //Descriptor set update
        void createDescriptorSetsforPipeline( VkDescriptorPool pool,  int MAX_FRAMES_IN_FLIGHT, perPipelineData* perPipelineData);
        void createPipelineLayoutForPipeline(perPipelineData* perPipelineData);
        void updateDescriptorSetsForPipeline(std::span<descriptorUpdateData> descriptorUpdates, uint32_t currentFrame, perPipelineData* perPipelineData);
        std::vector<VkWriteDescriptorSet> writeDescriptorSets;
        std::unordered_map<VkDescriptorSet, int> writeDescriptorSetsBindingIndices;
        
    };