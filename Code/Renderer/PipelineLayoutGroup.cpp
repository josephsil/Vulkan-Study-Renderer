
#include "PipelineLayoutGroup.h"
#include <cassert>

#include "rendererGlobals.h"
#include "vulkan-utilities.h"
#include "VulkanIncludes/Vulkan_Includes.h"

//TODO JS: break this dependency
#include <atldef.h>
#include <span>

#include "gpu-data-structs.h"
#include <General/MemoryArena.h>
#include <Renderer/RendererContext.h>


PipelineLayoutGroup::PipelineLayoutGroup(RendererContext handles, VkDescriptorPool pool,
    std::span<DescriptorSetRefs> RequiredGeneralDescriptorInfo,
    std::span<VkDescriptorSetLayoutBinding> PerPipelineDescriptorSetLayout, uint32_t perDrawSetsPerFrame,
    const char* debugName)
{
    device = handles.device;

    if (PerPipelineDescriptorSetLayout.size() != 0)
    {
        createLayout(handles, PerPipelineDescriptorSetLayout, debugName);
        initializePerShaderDescriptorSets(handles, pool, MAX_FRAMES_IN_FLIGHT, perDrawSetsPerFrame, debugName);
    }
    
    partialPipelinelayoutCreateInfo = {};
    Array layouts = MemoryArena::AllocSpan<VkDescriptorSetLayout>(handles.tempArena, RequiredGeneralDescriptorInfo.size() + 1);
    Array DependentSetsPerFrame = MemoryArena::AllocSpan<std::span<VkDescriptorSet>>(handles.arena, RequiredGeneralDescriptorInfo.size() + 1);
    for (auto requiredDescriptorSets : RequiredGeneralDescriptorInfo)
    {
        layouts.push_back(requiredDescriptorSets.layout);
        DependentSetsPerFrame.push_back(requiredDescriptorSets.perFrameSets);
        
    }
    pipelineData.RequiredGeneralPurposeDescriptorSets = DependentSetsPerFrame.getSpan();
    if (pipelineData.perShaderDescriptorSetLayout) layouts.push_back(pipelineData.perShaderDescriptorSetLayout);
    partialPipelinelayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    partialPipelinelayoutCreateInfo.setLayoutCount = (uint32_t)layouts.size(); 
    partialPipelinelayoutCreateInfo.pSetLayouts = layouts.getSpan().data();
    
}


void PipelineLayoutGroup::createLayout(RendererContext handles, 
    std::span<VkDescriptorSetLayoutBinding> PerPipelineDescriptorSetLayout, const char* debugName )
{
    if (PerPipelineDescriptorSetLayout.size() == 0)
    {
        this->pipelineData.perShaderDescriptorSetLayout = VK_NULL_HANDLE;
        this->pipelineData.slots= {};
        return;
    }
    //Create the layout for sets which are updated from here
    this->pipelineData.perShaderDescriptorSetLayout = DescriptorSets::createVkDescriptorSetLayout(handles, PerPipelineDescriptorSetLayout, debugName);
    this->pipelineData.slots = PerPipelineDescriptorSetLayout;
}



void bindDescriptorSet(VkCommandBuffer cmd, std::span<VkDescriptorSet> currentlyBoundSets, VkPipelineLayout layout, VkDescriptorSet set, uint32_t i, VkPipelineBindPoint bindPoint)
{
    // assert(set != currentlyBoundSets[i]);
    assert(layout != VK_NULL_HANDLE);
    if (currentlyBoundSets[i] ==  set)
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
void PipelineLayoutGroup::BindRequiredDescriptorSetsToCommandBuffer(VkCommandBuffer cmd, std::span<VkDescriptorSet> currentlyBoundSets, uint32_t currentFrame,  uint32_t descriptorOffset, VkPipelineBindPoint bindPoint)
{
    size_t i =0;
    //Bind all the general purpose descriptor sets required by this pipeline layout, early out if they're already bound
    for(i =0;  i < pipelineData.RequiredGeneralPurposeDescriptorSets.size(); i++)
    {
        auto descriptorSetDep = pipelineData.RequiredGeneralPurposeDescriptorSets[i];
        VkDescriptorSet set = VK_NULL_HANDLE;
        if (descriptorSetDep.size() == 1)
        {
            set = descriptorSetDep[0]; //This set is not per frame, it's global
        }
        else
        {
            set = descriptorSetDep[currentFrame];
        }
        bindDescriptorSet(cmd, currentlyBoundSets,  this->pipelineData.PipelineLayout, set, i, bindPoint);
    }

    //Bind the descriptors (if any) that are particular to this pipeline layout, early out if they're already bound
    if(pipelineData.descriptorSetsInitialized && pipelineData.pipelinesInitialized)
    {
        auto descriptorSet  = this->pipelineData.PerLayoutDescriptorSets[currentFrame][descriptorOffset];
        bindDescriptorSet(cmd, currentlyBoundSets,  this->pipelineData.PipelineLayout, descriptorSet, i, bindPoint);
    }
}


//updates descriptor sets based on vector of descriptorupdatedata, with some light validation that we're binding the right stuff
void PipelineLayoutGroup::UpdatePerPipelineDescriptorSets(RendererContext rendererHandles, std::span<descriptorUpdateData> descriptorUpdates, uint32_t currentFrame, uint32_t setToUpdate)
{
    if ( pipelineData.PerLayoutDescriptorSets.size() == 0)
    {
        return;
    }
    DescriptorSets::_updateDescriptorSet_NEW(rendererHandles, pipelineData.PerLayoutDescriptorSets[currentFrame][setToUpdate], pipelineData.slots,  descriptorUpdates);
}

void PipelineLayoutGroup::initializePerShaderDescriptorSets(RendererContext handles, VkDescriptorPool pool, int MAX_FRAMES_IN_FLIGHT,  uint32_t descriptorCt, const char* debugName)
{
    assert(!this->pipelineData.descriptorSetsInitialized); // Don't double initialize
    pipelineData.PerLayoutDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    for(int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        assert(pipelineData.perShaderDescriptorSetLayout != nullptr);
        pipelineData.PerLayoutDescriptorSets[i] = MemoryArena::AllocSpan<VkDescriptorSet>(handles.arena,descriptorCt);
        DescriptorSets::CreateDescriptorSetsForLayout(handles, pool, pipelineData.PerLayoutDescriptorSets[i], pipelineData.perShaderDescriptorSetLayout, descriptorCt, debugName);
    
    }
    pipelineData.descriptorSetsInitialized = true;
            
}

VkPipeline PipelineLayoutGroup::getPipeline(int index)
{
    //TODO JS
    // assert(pipelinesInitialized && pipelineLayoutInitialized);
    assert(index < pipelines.size());
    return pipelines[index];
}

uint32_t PipelineLayoutGroup::getPipelineCt()
{
    //TODO JS
    // assert(pipelinesInitialized && pipelineLayoutInitialized);
    return (uint32_t)pipelines.size();
}

void PipelineLayoutGroup::createPipelineLayoutForPipeline(perPipelineData* perPipelineData, size_t pconstantSize, char*name, bool compute)
{

    if(perPipelineData->pipelineLayoutInitialized)
    {
        return;
    }
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = partialPipelinelayoutCreateInfo; 
 
    //setup push constants
    VkPushConstantRange push_constant = {};
    //this push constant range starts at the beginning
    push_constant.offset = 0;
    //this push constant range takes up the size of a MeshPushConstants struct
    push_constant.size = (uint32_t)pconstantSize;
    
    push_constant.stageFlags = compute ? VK_SHADER_STAGE_COMPUTE_BIT : VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

  
        pipelineLayoutInfo.pPushConstantRanges = &push_constant;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
   

    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &perPipelineData->PipelineLayout) != VK_SUCCESS)
    {
        printf("failed to create pipeline layout!");
    exit(-1);
    }
    setDebugObjectName(device,VK_OBJECT_TYPE_PIPELINE_LAYOUT, name, (uint64_t)(perPipelineData->PipelineLayout));
    perPipelineData->pipelineLayoutInitialized = true;
  
}




void PipelineLayoutGroup::createGraphicsPipeline(std::span<VkPipelineShaderStageCreateInfo> shaders, char* name, graphicsPipelineSettings settings, size_t pconstantSize)
{
    auto pipeline =  &pipelineData;
    createPipelineLayoutForPipeline(pipeline, pconstantSize,name, false);
   
    assert(pipeline->pipelineLayoutInitialized);
    VkPipeline newGraphicsPipeline {}; 

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
    rasterizer.cullMode =  settings.cullMode;
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
    pipelineInfo.stageCount = (uint32_t)shaders.size(); //TODO JS?
    pipelineInfo.pStages = shaders.data();

    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;

    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    
    pipelineInfo.layout = pipeline->PipelineLayout;
    pipelineInfo.renderPass = VK_NULL_HANDLE;
    pipelineInfo.subpass = 0;

    const VkPipelineRenderingCreateInfo dynamicRenderingInfo {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
        .colorAttachmentCount = (uint32_t)settings.colorFormats.size(),
        .pColorAttachmentFormats = settings.colorFormats.size() ? settings.colorFormats.data() : VK_NULL_HANDLE ,
        .depthAttachmentFormat =  (VkFormat) (settings.depthFormat)
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
   
    setDebugObjectName(device, VK_OBJECT_TYPE_PIPELINE,name, uint64_t(newGraphicsPipeline));

    pipeline->pipelinesInitialized = true;
    pipelines.push_back(newGraphicsPipeline);
}

void PipelineLayoutGroup::createComputePipeline(VkPipelineShaderStageCreateInfo shader, char* name, size_t pconstantSize)
{

    auto pipeline =  &pipelineData;
    createPipelineLayoutForPipeline(pipeline, pconstantSize,name, true);
    assert(pipeline->pipelineLayoutInitialized);
    VkPipeline newPipeline {}; 
    
    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.layout =  pipeline->PipelineLayout;
    VkPipelineShaderStageCreateInfo computeShaderStageInfo = shader;


    
    pipelineInfo.stage = computeShaderStageInfo;
   

    VK_CHECK(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &newPipeline));

    setDebugObjectName(device, VK_OBJECT_TYPE_PIPELINE,"lines  shader", uint64_t(newPipeline));
    pipeline->pipelinesInitialized = true;
    pipelines.push_back(newPipeline);
}

void PipelineLayoutGroup::cleanup()
{
    
   
    for(int i = 0; i < pipelines.size(); i++)
    {
        vkDestroyPipeline(device, pipelines[i], nullptr);
    }

    pipelineData.cleanup(device);

}


void PipelineLayoutGroup::perPipelineData::cleanup(VkDevice device)
{
    vkDestroyPipelineLayout(device, PipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, perShaderDescriptorSetLayout, nullptr);
}
