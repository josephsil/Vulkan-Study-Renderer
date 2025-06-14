#pragma once

#include <span>
#include <unordered_map>
#include <vector>

#include "General/MemoryArena.h"
#include "Renderer/MainRenderer/PipelineManager/PipelineLayoutManager.h"
struct Vertex;
//Forward declaration
struct PerThreadRenderContext;
struct DescriptorUpdateData;


//Pipeline layout groups organize shaders, pipelines (and relaed resources) by Layout 
//The renderer mostly works with groups, and then queries them to get specific shaders when it's time 
//To batch and actually render. 
//This was initially implemented very early on in my understanding of vulkan, and has been since contorted into a slightly better state
//May need an entire rethink.
DescriptorDataForPipeline CreateDescriptorDataForPipeline(PerThreadRenderContext ctx, VkDescriptorSetLayout layout,
                                                          bool isPerFrame,
                                                          std::span<VkDescriptorSetLayoutBinding> bindingLayout,
                                                          const char* setname, VkDescriptorPool pool,
                                                          int descriptorSetPoolSize = 1);
class PipelineLayoutGroup
{
public:

	PipelineLayoutGroup();

	static void Initialize(PipelineLayoutGroup* target, PerThreadRenderContext handles, VkDescriptorPool pool,
					std::span<DescriptorDataForPipeline> descriptorInfo,
					std::span<VkDescriptorSetLayout> layouts, uint32_t pconstantsize, bool compute,
					const char* debugName);

    //Initialization

    void createPipeline(std::span<VkPipelineShaderStageCreateInfo> shaders, const char* name,
                        GraphicsPipelineSettings settings);
    void createGraphicsPipeline(std::span<VkPipelineShaderStageCreateInfo> shaders, char* name,
                                GraphicsPipelineSettings settings);
    void createComputePipeline(VkPipelineShaderStageCreateInfo shader, char* name, size_t pconstantSize = 0);
    struct PerPipelineLayoutData;

    //Runtime
    void BindRequiredDescriptorSetsToCommandBuffer(VkCommandBuffer cmd, std::span<VkDescriptorSet> currentlyBoundSets,
                                                   uint32_t currentFrame, VkPipelineBindPoint
                                                   bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS);

    VkPipeline getPipeline(size_t index);
    uint32_t getPipelineCt();
    void cleanup();


    struct PerPipelineLayoutData
    {
        bool iscompute;
        VkPipelineLayout layout;
        std::span<DescriptorDataForPipeline> descriptorConfiguration;
        void cleanup(VkDevice device);
    };

    PerPipelineLayoutData layoutData;
    VkPipelineLayoutCreateInfo partialPipelinelayoutCreateInfo; //Has the layouts, but not push constants
    // perPipelineData shadowPipelineData;
    //Descriptor set update

private:
	bool initialized;
    size_t lastFrame;
    VkDevice device;
    void createPipelineLayoutForGroup(PerPipelineLayoutData* perPipelineData, size_t pconstantSize, char* name, bool
                                      compute);
	std::vector<VkPipeline> pipelines;
    std::vector<VkWriteDescriptorSet> writeDescriptorSets;
    std::unordered_map<VkDescriptorSet, int> writeDescriptorSetsBindingIndices;
};
