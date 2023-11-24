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

    void AllocateDescriptorSet(RendererHandles handles, VkDescriptorPool pool, VkDescriptorSetLayout* pdescriptorsetLayout, VkDescriptorSet* pset);

    //Sorta material-esque -- should this grow, and own information about the corresponding pipeline?
    class bindlessDescriptorSetData
    {
    public:
        VkDescriptorSetLayout uniformDescriptorLayout;
        VkDescriptorSetLayout storageDescriptorLayout;
        VkDescriptorSetLayout imageDescriptorLayout;
        VkDescriptorSetLayout samplerDescriptorLayout;
        
        std::vector<layoutInfo> slots;

        std::vector<VkDescriptorSetLayout> getLayouts()
        {
            return {
                uniformDescriptorLayout,storageDescriptorLayout,imageDescriptorLayout,samplerDescriptorLayout
            };
        }

        void bindToCommandBuffer(VkCommandBuffer cmd, VkPipelineLayout layout, uint32_t currentFrame);
        void updateDescriptorSets(VkDevice device, std::vector<descriptorUpdateData> descriptorUpdates, uint32_t currentFrame);
        void createDescriptorSets(RendererHandles handles, VkDescriptorPool pool,  int MAX_FRAMES_IN_FLIGHT);

    private:
        bool descriptorSetsInitialized = false;
      
        
        std::vector<VkDescriptorSet> uniformDescriptorSetForFrame = {};
        std::vector<VkDescriptorSet> storageDescriptorSetForFrame = {};
        std::vector<VkDescriptorSet> imageDescriptorSetForFrame = {};
        std::vector<VkDescriptorSet> samplerDescriptorSetForFrame = {};


        //Descriptor set update
        std::vector<VkWriteDescriptorSet> writeDescriptorSets;
        std::unordered_map<VkDescriptorSet, int> writeDescriptorSetsBindingIndices;
        
        VkDescriptorSet getSetFromType(VkDescriptorType type, int currentFrame)
        {
            switch (type)
            {
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
                return uniformDescriptorSetForFrame[currentFrame];
                break;
            case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
                return imageDescriptorSetForFrame[currentFrame];
                break;
            case VK_DESCRIPTOR_TYPE_SAMPLER:
                return samplerDescriptorSetForFrame[currentFrame];
                break;
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
                return storageDescriptorSetForFrame[currentFrame];
                break;
            default:
                exit(-1);
            }
        }
        
    };

    bindlessDescriptorSetData createBindlessLayout(RendererHandles rendererHandles, Scene* scene);
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


    