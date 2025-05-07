#include "PipelineLayoutGroup.h"
#include <cassert>

#include "../../../rendererGlobals.h"
#include "../../../vulkan-utilities.h"
#include "../../../VulkanIncludes/Vulkan_Includes.h"

//TODO JS: break this dependency
#include <atldef.h>
#include <span>

#include "../../../gpu-data-structs.h"
#include <General/MemoryArena.h>
#include <Renderer/RendererContext.h>


//Descriptor set pool stuff: 
void PreAllocatedDescriptorSetPool::resetCacheForFrame()
{
    descriptorsUsed = SIZE_MAX;
    descriptorsUsed = 0;
}

VkDescriptorSet PreAllocatedDescriptorSetPool::getNextDescriptorSet()
{
    activeDescriptorSet = descriptorsUsed++;
    return descriptorSets[activeDescriptorSet];
}
VkDescriptorSet PreAllocatedDescriptorSetPool::peekNextDescriptorSet()
{
    return descriptorSets[descriptorsUsed];
}


DescriptorDataForPipeline constructDescriptorDataObject(MemoryArena::memoryArena* arena,
                                                        std::span<VkDescriptorSetLayoutBinding> layoutBindings,
                                                        uint32_t descriptorSetPoolSize, bool isPerFrame)
{
    int SetsForFrameCt = isPerFrame ? MAX_FRAMES_IN_FLIGHT : 1;
    PreAllocatedDescriptorSetPool* _DescriptorSets = MemoryArena::AllocSpan<PreAllocatedDescriptorSetPool>(
        arena, SetsForFrameCt).data();

    for (int i = 0; i < SetsForFrameCt; i++)
    {
        (_DescriptorSets[i]) = PreAllocatedDescriptorSetPool(arena, descriptorSetPoolSize);
    }

    auto _BindlessLayoutBindings = MemoryArena::copySpan<VkDescriptorSetLayoutBinding>(arena, layoutBindings);
    return {
        .isPerFrame = isPerFrame, .descriptorSetsCaches = _DescriptorSets, .layoutBindings = _BindlessLayoutBindings
    };
}



PipelineLayoutGroup::PipelineLayoutGroup(RendererContext handles, VkDescriptorPool pool,
                                         std::span<DescriptorDataForPipeline> descriptorInfo,
                                         std::span<VkDescriptorSetLayout> layouts, uint32_t pconstantsize, bool compute,
                                         const char* debugName)
{
    device = handles.device;

    partialPipelinelayoutCreateInfo = {};
    layoutData.descriptorConfiguration = copySpan(handles.arena, descriptorInfo);

    partialPipelinelayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    partialPipelinelayoutCreateInfo.setLayoutCount = static_cast<uint32_t>(layouts.size());
    partialPipelinelayoutCreateInfo.pSetLayouts = copySpan(handles.arena, layouts).data();
    createPipelineLayoutForGroup(&layoutData, pconstantsize, const_cast<char*>(debugName), compute);
    layoutData.iscompute = compute;
}


void bindDescriptorSet(VkCommandBuffer cmd, std::span<VkDescriptorSet> currentlyBoundSets, VkPipelineLayout layout,
                       VkDescriptorSet set, uint32_t i, VkPipelineBindPoint bindPoint)
{
    // assert(set != currentlyBoundSets[i]);
    assert(layout != VK_NULL_HANDLE);
    if (currentlyBoundSets[i] == set)
    {
        return;
    }
    currentlyBoundSets[i] = set;
    vkCmdBindDescriptorSets(cmd, bindPoint, layout,
                            i, 1,
                            &set, 0, nullptr);
}


//Descriptor offset: Currently I allocate all of my descriptor sets for the whole renderer up front, because there are very few
//The Mip Chain group  needs multiple descriptor sets, because it has to write to various mip levels, so the offset indices in 
//Likely that will need a revisit.

void PipelineLayoutGroup::BindRequiredDescriptorSetsToCommandBuffer(VkCommandBuffer cmd,
                                                                    std::span<VkDescriptorSet> currentlyBoundSets,
                                                                    uint32_t currentFrame,
                                                                    VkPipelineBindPoint bindPoint)
{
    //Bind all the general purpose descriptor sets required by this pipeline layout, early out if they're already bound
    for (size_t i = 0; i < layoutData.descriptorConfiguration.size(); i++)
    {
        auto descriptorData = layoutData.descriptorConfiguration[i];
        VkDescriptorSet set = {};
        size_t isPerFrameOffset = descriptorData.isPerFrame ? currentFrame : 0;
        PreAllocatedDescriptorSetPool* c = &descriptorData.descriptorSetsCaches[isPerFrameOffset];
        assert(c->activeDescriptorSet != SIZE_MAX, "Uninitialized descriptor pool");
        set = descriptorData.descriptorSetsCaches[isPerFrameOffset].descriptorSets[(c->activeDescriptorSet)];


        bindDescriptorSet(cmd, currentlyBoundSets, this->layoutData.layout, set, i, bindPoint);
    }
}


VkPipeline PipelineLayoutGroup::getPipeline(size_t index)
{
    assert(index < pipelines.size());
    return pipelines[index];
}

uint32_t PipelineLayoutGroup::getPipelineCt()
{
    return static_cast<uint32_t>(pipelines.size());
}

void PipelineLayoutGroup::createPipelineLayoutForGroup(PerPipelineLayoutData* perPipelineData, size_t pconstantSize,
                                                       char* name, bool compute)
{
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = partialPipelinelayoutCreateInfo;

    //setup push constants
    VkPushConstantRange push_constant = {};
    push_constant.offset = 0;
    push_constant.size = static_cast<uint32_t>(pconstantSize);

    push_constant.stageFlags = compute
                                   ? VK_SHADER_STAGE_COMPUTE_BIT
                                   : VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;


    pipelineLayoutInfo.pPushConstantRanges = &push_constant;
    pipelineLayoutInfo.pushConstantRangeCount = 1;


    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &perPipelineData->layout) != VK_SUCCESS)
    {
        printf("failed to create pipeline layout!");
        exit(-1);
    }
    setDebugObjectName(device, VK_OBJECT_TYPE_PIPELINE_LAYOUT, name, (uint64_t)(perPipelineData->layout));
}


void PipelineLayoutGroup::createPipeline(std::span<VkPipelineShaderStageCreateInfo> shaders, const char* name,
                                         graphicsPipelineSettings settings)
{
    bool compute = layoutData.iscompute;
    if (compute)
    {
        createComputePipeline(shaders[0], const_cast<char*>(name));
    }
    else
    {
        createGraphicsPipeline(shaders, const_cast<char*>(name), settings);
    }
}


void PipelineLayoutGroup::createGraphicsPipeline(std::span<VkPipelineShaderStageCreateInfo> shaders, char* name,
                                                 graphicsPipelineSettings settings)
{
    VkPipeline newGraphicsPipeline{};

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 0;
    vertexInputInfo.vertexAttributeDescriptionCount = 0;
    vertexInputInfo.pVertexBindingDescriptions = nullptr;
    vertexInputInfo.pVertexAttributeDescriptions = nullptr;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = settings.topology;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = settings.cullMode;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = settings.depthBias;
    // rasterizer.depthBiasConstantFactor = shadow ? 6 : 0;
    // rasterizer.depthBiasSlopeFactor = shadow ? 3 : 0;
    // rasterizer.depth

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = settings.depthWriteEnable;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    if (settings.dynamicBias)
    {
        dynamicStates.push_back(VK_DYNAMIC_STATE_DEPTH_BIAS);
    }
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = static_cast<uint32_t>(shaders.size()); //TODO JS?
    pipelineInfo.pStages = shaders.data();

    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;

    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;

    pipelineInfo.layout = layoutData.layout;
    pipelineInfo.renderPass = VK_NULL_HANDLE;
    pipelineInfo.subpass = 0;

    const VkPipelineRenderingCreateInfo dynamicRenderingInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
        .colorAttachmentCount = static_cast<uint32_t>(settings.colorFormats.size()),
        .pColorAttachmentFormats = settings.colorFormats.size() ? settings.colorFormats.data() : VK_NULL_HANDLE,
        .depthAttachmentFormat = settings.depthFormat
    };

    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
    pipelineInfo.basePipelineIndex = -1; // Optional
    pipelineInfo.pNext = &dynamicRenderingInfo;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &newGraphicsPipeline) !=
        VK_SUCCESS)
    {
        printf("failed to create graphics pipeline!");
        exit(-1);
    }

    setDebugObjectName(device, VK_OBJECT_TYPE_PIPELINE, name, uint64_t(newGraphicsPipeline));

    pipelines.push_back(newGraphicsPipeline);
}


void PipelineLayoutGroup::createComputePipeline(VkPipelineShaderStageCreateInfo shader, char* name,
                                                size_t pconstantSize)
{
    VkPipeline newPipeline{};

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.layout = layoutData.layout;
    VkPipelineShaderStageCreateInfo computeShaderStageInfo = shader;


    pipelineInfo.stage = computeShaderStageInfo;


    VK_CHECK(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &newPipeline));

    setDebugObjectName(device, VK_OBJECT_TYPE_PIPELINE, name, uint64_t(newPipeline));
    pipelines.push_back(newPipeline);
}

void PipelineLayoutGroup::cleanup()
{
    for (int i = 0; i < pipelines.size(); i++)
    {
        vkDestroyPipeline(device, pipelines[i], nullptr);
    }

    layoutData.cleanup(device);
}


void PipelineLayoutGroup::PerPipelineLayoutData::cleanup(VkDevice device)
{
    vkDestroyPipelineLayout(device, layout, nullptr);
    // vkDestroyDescriptorSetLayout(device, perShaderDescriptorSetLayout, nullptr);
}


