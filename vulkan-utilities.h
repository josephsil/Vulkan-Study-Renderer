#pragma once
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan_core.h>

#include "forward-declarations-renderer.h"
#include "common-structs.h"


struct Vertex;
class Scene;
//Forward declaration
struct RendererHandles;
struct CommandPoolManager;
struct TextureData;


struct descriptorUpdateData
{
    VkDescriptorType type;
    void* ptr;
    uint32_t count = 1;
};

struct dataBuffer
{
    VkBuffer data;
    uint32_t size;

    VkDescriptorBufferInfo getBufferInfo();
};

namespace DescriptorSets
{
    struct layoutInfo;
    std::pair<std::vector<VkDescriptorImageInfo>, std::vector<VkDescriptorImageInfo>> ImageInfoFromImageDataVec(std::vector<TextureData> textures);
    
    //Passing around a vector of these to enforce binding for a pipeline
    struct layoutInfo
    {
        VkDescriptorType type;
        uint32_t slot;
        uint32_t descriptorCount = 1;
    };

    void AllocateDescriptorSet(VkDevice device, VkDescriptorPool pool, VkDescriptorSetLayout* pdescriptorsetLayout, VkDescriptorSet* pset);

    //Sorta material-esque -- should this grow, and own information about the corresponding pipeline?
    class bindlessDrawData
    {
    public:
        bindlessDrawData()
        {}
        bindlessDrawData(RendererHandles handles, Scene* pscene);

        //Initialization
       
        void createGraphicsPipeline(std::vector<VkPipelineShaderStageCreateInfo> shaders, VkFormat* swapchainFormat, VkFormat* depthFormat, bool shadow, bool color = true, bool
                                    depth = true);

        //Runtime
        void updateOpaqueDescriptorSets(std::vector<descriptorUpdateData> descriptorUpdates, uint32_t currentFrame);
        void updateShadowDescriptorSets(std::vector<descriptorUpdateData> descriptorUpdates, uint32_t currentFrame);
        void createDescriptorSetsOpaque(VkDescriptorPool pool, int MAX_FRAMES_IN_FLIGHT);
        void createDescriptorSetsShadow(VkDescriptorPool pool, int MAX_FRAMES_IN_FLIGHT);
        void bindToCommandBufferOpaque(VkCommandBuffer cmd, uint32_t currentFrame);
        void bindToCommandBufferShadow(VkCommandBuffer cmd, uint32_t currentFrame);

        VkPipeline getPipeline(int index);
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
            std::vector<layoutInfo> slots;

            bool pipelineLayoutInitialized = false;
            bool pipelinesInitialized = false;
            bool descriptorSetsInitialized = false;

            
            VkPipelineLayout bindlessPipelineLayout;
            VkDescriptorSetLayout uniformDescriptorLayout = {};
            VkDescriptorSetLayout storageDescriptorLayout = {};
            VkDescriptorSetLayout imageDescriptorLayout = {};
            VkDescriptorSetLayout samplerDescriptorLayout = {};
        
            std::vector<VkDescriptorSet> uniformDescriptorSetForFrame = {};
            std::vector<VkDescriptorSet> storageDescriptorSetForFrame = {};
            std::vector<VkDescriptorSet> imageDescriptorSetForFrame = {};
            std::vector<VkDescriptorSet> samplerDescriptorSetForFrame = {};

            VkDescriptorSet getSetFromType(VkDescriptorType type, int currentFrame);
            void cleanup(VkDevice device)
            {
                vkDestroyPipelineLayout(device, bindlessPipelineLayout, nullptr);
                vkDestroyDescriptorSetLayout(device, uniformDescriptorLayout, nullptr);
                vkDestroyDescriptorSetLayout(device, imageDescriptorLayout, nullptr);
                vkDestroyDescriptorSetLayout(device, samplerDescriptorLayout, nullptr);
                vkDestroyDescriptorSetLayout(device, storageDescriptorLayout, nullptr);
            }
        };

        perPipelineData opaquePipelineData;
        perPipelineData shadowPipelineData;

        void bindToCommandBuffer(VkCommandBuffer cmd, uint32_t currentFrame,perPipelineData* pipelinedata);
        void createOpaqueLayout(RendererHandles handles, Scene* pscene);
        void createShadowLayout(RendererHandles handles, Scene* pscene);


        //Descriptor set update
        void createDescriptorSetsforPipeline( VkDescriptorPool pool,  int MAX_FRAMES_IN_FLIGHT, perPipelineData* perPipelineData);
        void createPipelineLayoutForPipeline(perPipelineData* perPipelineData);
        void updateDescriptorSetsForPipeline(std::vector<descriptorUpdateData> descriptorUpdates, uint32_t currentFrame, perPipelineData* perPipelineData);
        std::vector<VkWriteDescriptorSet> writeDescriptorSets;
        std::unordered_map<VkDescriptorSet, int> writeDescriptorSetsBindingIndices;
        
    };

}

namespace Capabilities
{
    VkFormat findDepthFormat(RendererHandles rendererHandles);

    VkFormat findSupportedFormat(RendererHandles rendererHandles, const std::vector<VkFormat>& candidates,
                                 VkImageTiling tiling,
                                 VkFormatFeatureFlags features);    
    uint32_t findMemoryType(RendererHandles rendererHandles, uint32_t typeFilter, VkMemoryPropertyFlags properties);
}


    