#include "PipelineDataObject.h"

#include <cassert>

#include "rendererGlobals.h"
#include "vulkan-utilities.h"
#include "VulkanIncludes/Vulkan_Includes.h"

//TODO JS: break this dependency
#include <span>

#include "gpu-data-structs.h"
#include "../General/Memory.h"


VkDescriptorSetLayoutCreateInfo createInfoFromSpan( std::span<VkDescriptorSetLayoutBinding> bindings)
{
    VkDescriptorSetLayoutCreateInfo _createInfo = {};
    _createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    // _createInfo.flags =   VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
    _createInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    _createInfo.pBindings = bindings.data();

    return _createInfo;
}
PipelineDataObject::PipelineDataObject(RendererHandles handles, VkDescriptorPool pool, std::span<VkDescriptorSetLayoutBinding> opaqueLayout)
{
    device = handles.device;
    createLayout(handles , opaqueLayout);
    createDescriptorSets(pool, MAX_FRAMES_IN_FLIGHT);
    
}

void PipelineDataObject::createLayout(RendererHandles handles,  std::span<VkDescriptorSetLayoutBinding> layout )
{
    VkDescriptorSetLayoutCreateInfo perSceneLaout = createInfoFromSpan(layout);


    VkDescriptorBindingFlags binding_flags = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT; 
    VkDescriptorSetLayoutBindingFlagsCreateInfoEXT extended_info = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT};
    extended_info.bindingCount = layout.size(); 

    std::span<VkDescriptorBindingFlagsEXT> extFlags = MemoryArena::AllocSpan<VkDescriptorBindingFlagsEXT>(handles.perframeArena, layout.size());

    //enable partially bound bit for all samplers and images
    for (int i = 0; i < extFlags.size(); i++)
    {
        extFlags[i] = (layout[i].descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE || layout[i].descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER) ? binding_flags : 0;
    }
   
    extended_info.pBindingFlags = extFlags.data();
    perSceneLaout.pNext = &extended_info;


    
    VK_CHECK(vkCreateDescriptorSetLayout(handles.device, &perSceneLaout, nullptr, &this->pipelineData.perSceneDescriptorSetLayout));
    this->pipelineData.slots = layout;
}

void PipelineDataObject::bindToCommandBuffer(VkCommandBuffer cmd, uint32_t currentFrame, VkPipelineBindPoint bindPoint)
{
    assert(pipelineData.descriptorSetsInitialized && pipelineData.pipelinesInitialized);
    vkCmdBindDescriptorSets(cmd, bindPoint, this->pipelineData.bindlessPipelineLayout, 0, 1, &this->pipelineData.perSceneDescriptorSetForFrame[currentFrame], 0, nullptr);
}



void PipelineDataObject::updateDescriptorSetsForPipeline(std::span<descriptorUpdateData> descriptorUpdates, uint32_t currentFrame, perPipelineData* perPipelineData)
{
    
    writeDescriptorSets.clear();
    writeDescriptorSetsBindingIndices.clear();
    
    for(int i = 0; i < descriptorUpdates.size(); i++)
    {
        descriptorUpdateData update = descriptorUpdates[i];
        VkDescriptorSet set = perPipelineData-> perSceneDescriptorSetForFrame[currentFrame];
        
        if (!writeDescriptorSetsBindingIndices.contains(set))
        {
            writeDescriptorSetsBindingIndices[set] = 0;
        }

        VkDescriptorSetLayoutBinding bindingInfo = perPipelineData->slots[writeDescriptorSets.size()];

        VkWriteDescriptorSet newSet = {};
        newSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        newSet.dstBinding = bindingInfo.binding;
        newSet.dstSet = set;
        newSet.descriptorCount = update.count;
        newSet.descriptorType = update.type;

        assert(update.type == bindingInfo.descriptorType);
        assert(update.count <= bindingInfo.descriptorCount);
        // assert(bindingIndex == bindingInfo.binding);

        if (update.type == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE || update.type == VK_DESCRIPTOR_TYPE_SAMPLER)
        {
            newSet.pImageInfo = static_cast<VkDescriptorImageInfo*>(update.ptr);
        }
        else
        {
            newSet.pBufferInfo = static_cast<VkDescriptorBufferInfo*>(update.ptr);
        }
    
        writeDescriptorSets.push_back(newSet);
        
        writeDescriptorSetsBindingIndices[set]++;
    }
    
    assert(writeDescriptorSets.size() == perPipelineData->slots.size());

    vkUpdateDescriptorSets(device, writeDescriptorSets.size(), writeDescriptorSets.data(), 0, nullptr);
}

//updates descriptor sets based on vector of descriptorupdatedata, with some light validation that we're binding the right stuff
void PipelineDataObject::updateDescriptorSets(std::span<descriptorUpdateData> descriptorUpdates, uint32_t currentFrame)
{
   updateDescriptorSetsForPipeline(descriptorUpdates, currentFrame, &pipelineData);
}




void PipelineDataObject::createDescriptorSets(VkDescriptorPool pool, int MAX_FRAMES_IN_FLIGHT)
{
    assert(!this->pipelineData.descriptorSetsInitialized); // Don't double initialize
    pipelineData.perSceneDescriptorSetForFrame.resize(MAX_FRAMES_IN_FLIGHT);
    for(int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        if ( pipelineData.perSceneDescriptorSetLayout != nullptr) DescriptorSets::AllocateDescriptorSet(device, pool,  & pipelineData.perSceneDescriptorSetLayout, & pipelineData.perSceneDescriptorSetForFrame[i]);
    }
    pipelineData.descriptorSetsInitialized = true;
            
}

VkPipeline PipelineDataObject::getPipeline(int index)
{
    //TODO JS
    // assert(pipelinesInitialized && pipelineLayoutInitialized);
    assert(index < pipelines.size());
    return pipelines[index];
}

uint32_t PipelineDataObject::getPipelineCt()
{
    //TODO JS
    // assert(pipelinesInitialized && pipelineLayoutInitialized);
    return pipelines.size();
}

void PipelineDataObject::createPipelineLayoutForPipeline(perPipelineData* perPipelineData, size_t pconstantSize, bool compute)
{

    if(perPipelineData->pipelineLayoutInitialized)
    {
        return;
    }
    
    std::vector<VkDescriptorSetLayout> layouts ={};
    if (perPipelineData->perSceneDescriptorSetLayout) layouts.push_back(perPipelineData->perSceneDescriptorSetLayout);
    
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = layouts.size(); // Optional
    pipelineLayoutInfo.pSetLayouts = layouts.data();

    if (pconstantSize != 0)
    {
    //setup push constants
    VkPushConstantRange push_constant;
    //this push constant range starts at the beginning
    push_constant.offset = 0;
    //this push constant range takes up the size of a MeshPushConstants struct
    push_constant.size = pconstantSize;
    
    push_constant.stageFlags = compute ? VK_SHADER_STAGE_COMPUTE_BIT : VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

  
        pipelineLayoutInfo.pPushConstantRanges = &push_constant;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
    }

    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &perPipelineData->bindlessPipelineLayout) != VK_SUCCESS)
    {
        printf("failed to create pipeline layout!");
    exit(-1);
    }
    perPipelineData->pipelineLayoutInitialized = true;
}



void PipelineDataObject::createGraphicsPipeline(std::vector<VkPipelineShaderStageCreateInfo> shaders, graphicsPipelineSettings settings, size_t pconstantSize)
{
    auto pipeline =  &pipelineData;
    createPipelineLayoutForPipeline(pipeline, pconstantSize,false);
    
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
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
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
    pipelineInfo.stageCount = shaders.size(); //TODO JS?
    pipelineInfo.pStages = shaders.data();

    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;

    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    
    pipelineInfo.layout = pipeline->bindlessPipelineLayout;
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

    pipeline->pipelinesInitialized = true;
    pipelines.push_back(newGraphicsPipeline);
}

void PipelineDataObject::createComputePipeline(VkPipelineShaderStageCreateInfo shader, size_t pconstantSize)
{

    auto pipeline =  &pipelineData;
    createPipelineLayoutForPipeline(pipeline, pconstantSize,true);
    assert(pipeline->pipelineLayoutInitialized);
    VkPipeline newPipeline {}; 
    
    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.layout =  pipeline->bindlessPipelineLayout;
    VkPipelineShaderStageCreateInfo computeShaderStageInfo = shader;


    
    pipelineInfo.stage = computeShaderStageInfo;
   

    VK_CHECK(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &newPipeline));
    
    pipeline->pipelinesInitialized = true;
    pipelines.push_back(newPipeline);
}

void PipelineDataObject::cleanup()
{
    
   
    for(int i = 0; i < pipelines.size(); i++)
    {
        vkDestroyPipeline(device, pipelines[i], nullptr);
    }

    pipelineData.cleanup(device);

}

VkDescriptorSet PipelineDataObject::perPipelineData::getSetFromType(VkDescriptorType type,
    int currentFrame)
{
    printf("Deprecated call to getSetFromType \n");
    return perSceneDescriptorSetForFrame[currentFrame];
  
}

void PipelineDataObject::perPipelineData::cleanup(VkDevice device)
{
    vkDestroyPipelineLayout(device, bindlessPipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, perSceneDescriptorSetLayout, nullptr);
}
