
#include "PipelineGroup.h"
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


VkDescriptorSetLayoutCreateInfo createInfoFromSpan( std::span<VkDescriptorSetLayoutBinding> bindings)
{
    VkDescriptorSetLayoutCreateInfo _createInfo = {};
    _createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    // _createInfo.flags =   VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
    _createInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    _createInfo.pBindings = bindings.data();

    return _createInfo;
}
PipelineGroup::PipelineGroup(RendererContext handles, VkDescriptorPool pool, std::span<VkDescriptorSetLayoutBinding> opaqueLayout, uint32_t setsPerFrame, const char* debugName)
{
    device = handles.device;
    createLayout(handles , opaqueLayout, debugName);
    createDescriptorSets(handles, pool, MAX_FRAMES_IN_FLIGHT, setsPerFrame, debugName);
}

void PipelineGroup::createLayout(RendererContext handles,  std::span<VkDescriptorSetLayoutBinding> layout, const char* debugName )
{
    VkDescriptorSetLayoutCreateInfo perSceneLaout = createInfoFromSpan(layout);


    VkDescriptorBindingFlags binding_flags = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT; 
    VkDescriptorSetLayoutBindingFlagsCreateInfoEXT extended_info = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT};
    extended_info.bindingCount = (uint32_t)layout.size(); 

    std::span<VkDescriptorBindingFlagsEXT> extFlags = MemoryArena::AllocSpan<VkDescriptorBindingFlagsEXT>(handles.tempArena, layout.size());

    //enable partially bound bit for all samplers and images
    for (int i = 0; i < extFlags.size(); i++)
    {
        extFlags[i] = (layout[i].descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE || layout[i].descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER) ? binding_flags : 0;
    }
   
    extended_info.pBindingFlags = extFlags.data();
    perSceneLaout.pNext = &extended_info;


    
    VK_CHECK(vkCreateDescriptorSetLayout(handles.device, &perSceneLaout, nullptr, &this->pipelineData.perSceneDescriptorSetLayout));
    setDebugObjectName(handles.device, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, debugName,(uint64_t)this->pipelineData.perSceneDescriptorSetLayout );
    this->pipelineData.slots = layout;
}

//Descriptor offset: Currently I allocate all of my descriptor sets for the whole renderer up front, because there are very few
//The Mip Chain group  needs multiple descriptor sets, because it has to write to various mip levels, so the offset indices in 
//Likely that will need a revisit.
void PipelineGroup::bindToCommandBuffer(VkCommandBuffer cmd, uint32_t currentFrame,  uint32_t descriptorOffset, VkPipelineBindPoint bindPoint)
{
    assert(pipelineData.descriptorSetsInitialized && pipelineData.pipelinesInitialized);
    vkCmdBindDescriptorSets(cmd, bindPoint, this->pipelineData.bindlessPipelineLayout,
        0, 1,
        &this->pipelineData.perSceneDescriptorSetForFrame[currentFrame][descriptorOffset], 0, nullptr);
}



//Descriptor offset: Currently I allocate all of my descriptor sets for the whole renderer up front, because there are very few
//The Mip Chain group  needs multiple descriptor sets, because it has to write to various mip levels, so the offset indices in 
//Likely that will need a revisit.
void PipelineGroup::updateDescriptorSetsForPipeline(std::span<descriptorUpdateData> descriptorUpdates, uint32_t currentFrame, perPipelineData* perPipelineData, uint32_t descriptorOffset)
{
    
    writeDescriptorSets.clear();
    writeDescriptorSetsBindingIndices.clear();
    
    for(int i = 0; i < descriptorUpdates.size(); i++)
    {
        descriptorUpdateData update = descriptorUpdates[i];
        VkDescriptorSet set = perPipelineData-> perSceneDescriptorSetForFrame[currentFrame][descriptorOffset];
        
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

        if (update.type == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE || update.type == VK_DESCRIPTOR_TYPE_SAMPLER || update.type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
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

    vkUpdateDescriptorSets(device, (uint32_t)writeDescriptorSets.size(), writeDescriptorSets.data(), 0, nullptr);
}

//updates descriptor sets based on vector of descriptorupdatedata, with some light validation that we're binding the right stuff
void PipelineGroup::updateDescriptorSets(std::span<descriptorUpdateData> descriptorUpdates, uint32_t currentFrame, uint32_t descriptorToUpdate)
{
   updateDescriptorSetsForPipeline(descriptorUpdates, currentFrame, &pipelineData, descriptorToUpdate);
}




void PipelineGroup::createDescriptorSets(RendererContext handles, VkDescriptorPool pool, int MAX_FRAMES_IN_FLIGHT, uint32_t descriptorCt, const char* debugName)
{
    assert(!this->pipelineData.descriptorSetsInitialized); // Don't double initialize
    pipelineData.perSceneDescriptorSetForFrame.resize(MAX_FRAMES_IN_FLIGHT);
    for(int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        assert(pipelineData.perSceneDescriptorSetLayout != nullptr);
        pipelineData.perSceneDescriptorSetForFrame[i] = MemoryArena::AllocSpan<VkDescriptorSet>(handles.arena,descriptorCt);

        //Allocate descriptor sets expects one layout per each descriptor set allocated
        //(I think the expectation is you're allocating big chunks of un-sorted descriptor sets)
        //For now, since this only happens once at the start of the frame, we'll just copy the layout for the alloc
        //However this may need a revisit in the future
        auto descriptorSetLayoutCopiesForAlloc = MemoryArena::AllocSpan<VkDescriptorSetLayout>(handles.tempArena,descriptorCt);

        for(int j = 0; j < descriptorCt; j++)
        {
            descriptorSetLayoutCopiesForAlloc[j] = pipelineData.perSceneDescriptorSetLayout;
        }
        DescriptorSets::AllocateDescriptorSet(device, pool,  descriptorSetLayoutCopiesForAlloc.data(), pipelineData.perSceneDescriptorSetForFrame[i].data(), descriptorCt);

        for (auto element : pipelineData.perSceneDescriptorSetForFrame[i])
        {
            setDebugObjectName(device, VK_OBJECT_TYPE_DESCRIPTOR_SET,debugName, uint64_t(element) );
        }
    
    }
    pipelineData.descriptorSetsInitialized = true;
            
}

VkPipeline PipelineGroup::getPipeline(int index)
{
    //TODO JS
    // assert(pipelinesInitialized && pipelineLayoutInitialized);
    assert(index < pipelines.size());
    return pipelines[index];
}

uint32_t PipelineGroup::getPipelineCt()
{
    //TODO JS
    // assert(pipelinesInitialized && pipelineLayoutInitialized);
    return (uint32_t)pipelines.size();
}

void PipelineGroup::createPipelineLayoutForPipeline(perPipelineData* perPipelineData, size_t pconstantSize, bool compute)
{

    if(perPipelineData->pipelineLayoutInitialized)
    {
        return;
    }
    
    std::vector<VkDescriptorSetLayout> layouts ={};
    if (perPipelineData->perSceneDescriptorSetLayout) layouts.push_back(perPipelineData->perSceneDescriptorSetLayout);
    
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = (uint32_t)layouts.size(); // Optional
    pipelineLayoutInfo.pSetLayouts = layouts.data();

   
    //setup push constants
    VkPushConstantRange push_constant = {};
    //this push constant range starts at the beginning
    push_constant.offset = 0;
    //this push constant range takes up the size of a MeshPushConstants struct
    push_constant.size = (uint32_t)pconstantSize;
    
    push_constant.stageFlags = compute ? VK_SHADER_STAGE_COMPUTE_BIT : VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

  
        pipelineLayoutInfo.pPushConstantRanges = &push_constant;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
   

    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &perPipelineData->bindlessPipelineLayout) != VK_SUCCESS)
    {
        printf("failed to create pipeline layout!");
    exit(-1);
    }
    perPipelineData->pipelineLayoutInitialized = true;
  
}



void PipelineGroup::createGraphicsPipeline(std::span<VkPipelineShaderStageCreateInfo> shaders, char* name, graphicsPipelineSettings settings, size_t pconstantSize)
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
   
    setDebugObjectName(device, VK_OBJECT_TYPE_PIPELINE,"lines  shader", uint64_t(newGraphicsPipeline));

    pipeline->pipelinesInitialized = true;
    pipelines.push_back(newGraphicsPipeline);
}

void PipelineGroup::createComputePipeline(VkPipelineShaderStageCreateInfo shader, char* name, size_t pconstantSize)
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

    setDebugObjectName(device, VK_OBJECT_TYPE_PIPELINE,"lines  shader", uint64_t(newPipeline));
    pipeline->pipelinesInitialized = true;
    pipelines.push_back(newPipeline);
}

void PipelineGroup::cleanup()
{
    
   
    for(int i = 0; i < pipelines.size(); i++)
    {
        vkDestroyPipeline(device, pipelines[i], nullptr);
    }

    pipelineData.cleanup(device);

}


void PipelineGroup::perPipelineData::cleanup(VkDevice device)
{
    vkDestroyPipelineLayout(device, bindlessPipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, perSceneDescriptorSetLayout, nullptr);
}
