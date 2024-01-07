#include "PipelineDataObject.h"

#include <cassert>

#include "rendererGlobals.h"
#include "LayoutsBuilder.h"
#include "vulkan-utilities.h"
#include "VulkanIncludes/Vulkan_Includes.h"

//TODO JS: break this dependency
#include "gpu-data-structs.h"

PipelineDataObject::PipelineDataObject(RendererHandles handles, Scene* pscene)
{
    device = handles.device;
    createOpaqueLayout(handles , pscene);
    createShadowLayout(handles , pscene);
}

void PipelineDataObject::createOpaqueLayout(RendererHandles handles, Scene* pscene)
{
    LayoutsBuilder builder = createBindlessLayoutBuilder(handles,  pscene);
    VkDescriptorSetLayoutCreateInfo uniformDescriptorLayout =  builder.getCreateInfo(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    VkDescriptorSetLayoutCreateInfo imageDescriptorLayout = builder.getCreateInfo(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
    VkDescriptorSetLayoutCreateInfo samplerDescriptorLayout = builder.getCreateInfo(VK_DESCRIPTOR_TYPE_SAMPLER);
    VkDescriptorSetLayoutCreateInfo storageDescriptorLayout = builder.getCreateInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);


    VkDescriptorBindingFlags binding_flags = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT; 
    VkDescriptorSetLayoutBindingFlagsCreateInfoEXT extended_info = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT};
    extended_info.bindingCount = 3; 
    // In this example, the first binding is a UBO and the second is a combined image sampler array, so it only needs to be set on the second binding in my case. 
    VkDescriptorBindingFlagsEXT descriptor_binding_flags[3] = {
        binding_flags,
        binding_flags,
        binding_flags
       }; 
    extended_info.pBindingFlags = descriptor_binding_flags;
    imageDescriptorLayout.pNext = &extended_info;
    samplerDescriptorLayout.pNext = &extended_info;

    
    VK_CHECK(vkCreateDescriptorSetLayout(handles.device, &uniformDescriptorLayout, nullptr, &this->opaquePipelineData.uniformDescriptorLayout));
    VK_CHECK(vkCreateDescriptorSetLayout(handles.device, &imageDescriptorLayout, nullptr, &this->opaquePipelineData.imageDescriptorLayout));
    VK_CHECK(vkCreateDescriptorSetLayout(handles.device, &samplerDescriptorLayout, nullptr, &this->opaquePipelineData.samplerDescriptorLayout));
    VK_CHECK(vkCreateDescriptorSetLayout(handles.device, &storageDescriptorLayout, nullptr, &this->opaquePipelineData.storageDescriptorLayout));
    this->opaquePipelineData.slots = builder.bindings;
}
void PipelineDataObject::createShadowLayout(RendererHandles handles, Scene* pscene)
{
    LayoutsBuilder builder = createShadowLayoutBuilder(handles,  pscene); //TODO JS: different layout
    VkDescriptorSetLayoutCreateInfo uniformDescriptorLayout =  builder.getCreateInfo(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    VkDescriptorSetLayoutCreateInfo storageDescriptorLayout = builder.getCreateInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

    VK_CHECK(vkCreateDescriptorSetLayout(handles.device, &uniformDescriptorLayout, nullptr, &this->shadowPipelineData.uniformDescriptorLayout));
    VK_CHECK(vkCreateDescriptorSetLayout(handles.device, &storageDescriptorLayout, nullptr, &this->shadowPipelineData.storageDescriptorLayout));
    this->shadowPipelineData.slots = builder.bindings;
}

void PipelineDataObject::bindToCommandBufferOpaque(VkCommandBuffer cmd, uint32_t currentFrame)
{
    assert(opaquePipelineData.descriptorSetsInitialized && opaquePipelineData.pipelinesInitialized);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, opaquePipelineData.bindlessPipelineLayout, 0, 1, &opaquePipelineData.uniformDescriptorSetForFrame[currentFrame], 0, nullptr);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, opaquePipelineData.bindlessPipelineLayout, 1, 1, &opaquePipelineData.storageDescriptorSetForFrame[currentFrame], 0, nullptr);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, opaquePipelineData.bindlessPipelineLayout, 2, 1, &opaquePipelineData.imageDescriptorSetForFrame[currentFrame], 0, nullptr);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, opaquePipelineData.bindlessPipelineLayout, 3, 1, &opaquePipelineData.samplerDescriptorSetForFrame[currentFrame], 0, nullptr);
    
}

void PipelineDataObject::bindToCommandBufferShadow(VkCommandBuffer cmd, uint32_t currentFrame)
{
    assert(shadowPipelineData.descriptorSetsInitialized && shadowPipelineData.pipelinesInitialized);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowPipelineData.bindlessPipelineLayout, 0, 1, &shadowPipelineData.uniformDescriptorSetForFrame[currentFrame], 0, nullptr);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowPipelineData.bindlessPipelineLayout, 1, 1, &shadowPipelineData.storageDescriptorSetForFrame[currentFrame], 0, nullptr);

}


void PipelineDataObject::updateDescriptorSetsForPipeline(std::vector<descriptorUpdateData> descriptorUpdates, uint32_t currentFrame, perPipelineData* perPipelineData)
{
    
    writeDescriptorSets.clear();
    writeDescriptorSetsBindingIndices.clear();
    
    for(int i = 0; i < descriptorUpdates.size(); i++)
    {
        descriptorUpdateData update = descriptorUpdates[i];
        VkDescriptorSet set = perPipelineData->getSetFromType(descriptorUpdates[i].type, currentFrame);
        
        if (!writeDescriptorSetsBindingIndices.contains(set))
        {
            writeDescriptorSetsBindingIndices[set] = 0;
        }
        
        layoutInfo info = perPipelineData->slots[writeDescriptorSets.size()];

        int bindingIndex = writeDescriptorSetsBindingIndices[set];
        VkWriteDescriptorSet newSet = {};
        newSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        newSet.dstBinding = bindingIndex;
        newSet.dstSet = set;
        newSet.descriptorCount = update.count;
        newSet.descriptorType = update.type;

        assert(update.type == info.type);
        assert(update.count == info.descriptorCount);
        assert(bindingIndex == info.slot);

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
void PipelineDataObject::updateOpaqueDescriptorSets(std::vector<descriptorUpdateData> descriptorUpdates, uint32_t currentFrame)
{
   updateDescriptorSetsForPipeline(descriptorUpdates, currentFrame, &opaquePipelineData);
}

void PipelineDataObject::updateShadowDescriptorSets(std::vector<descriptorUpdateData> descriptorUpdates, uint32_t currentFrame)
{
    updateDescriptorSetsForPipeline(descriptorUpdates, currentFrame, &shadowPipelineData);
}


void PipelineDataObject::createDescriptorSetsOpaque(VkDescriptorPool pool, int MAX_FRAMES_IN_FLIGHT)
{
    createDescriptorSetsforPipeline(pool, MAX_FRAMES_IN_FLIGHT, &opaquePipelineData);
}

void PipelineDataObject::createDescriptorSetsShadow(VkDescriptorPool pool, int MAX_FRAMES_IN_FLIGHT)
{
    createDescriptorSetsforPipeline(pool, MAX_FRAMES_IN_FLIGHT, &shadowPipelineData);
}
void PipelineDataObject::createDescriptorSetsforPipeline(VkDescriptorPool pool, int MAX_FRAMES_IN_FLIGHT, perPipelineData* _perPipelineData)
{
    assert(!_perPipelineData->descriptorSetsInitialized); // Don't double initialize
    _perPipelineData->uniformDescriptorSetForFrame.resize(MAX_FRAMES_IN_FLIGHT);
    _perPipelineData->imageDescriptorSetForFrame.resize(MAX_FRAMES_IN_FLIGHT);
    _perPipelineData->samplerDescriptorSetForFrame.resize(MAX_FRAMES_IN_FLIGHT);
    _perPipelineData->storageDescriptorSetForFrame.resize(MAX_FRAMES_IN_FLIGHT);
    for(int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        if (_perPipelineData->uniformDescriptorLayout != nullptr) DescriptorSets::AllocateDescriptorSet(device, pool,  &_perPipelineData->uniformDescriptorLayout, &_perPipelineData->uniformDescriptorSetForFrame[i]);
        if (_perPipelineData->imageDescriptorLayout != nullptr) DescriptorSets::AllocateDescriptorSet(device, pool,  &_perPipelineData->imageDescriptorLayout, &_perPipelineData->imageDescriptorSetForFrame[i]);
        if (_perPipelineData->samplerDescriptorLayout != nullptr) DescriptorSets::AllocateDescriptorSet(device, pool,  &_perPipelineData->samplerDescriptorLayout, &_perPipelineData->samplerDescriptorSetForFrame[i]);
        if (_perPipelineData->storageDescriptorLayout != nullptr) DescriptorSets::AllocateDescriptorSet(device, pool,  &_perPipelineData->storageDescriptorLayout, &_perPipelineData->storageDescriptorSetForFrame[i]);
    }
    _perPipelineData->descriptorSetsInitialized = true;
            
}

VkPipeline PipelineDataObject::getPipeline(int index)
{
    //TODO JS
    // assert(pipelinesInitialized && pipelineLayoutInitialized);
    assert(index < bindlesspipelineObjects.size());
    return bindlesspipelineObjects[index];
}

VkPipelineLayout PipelineDataObject::getLayoutOpaque()
{
    return opaquePipelineData.bindlessPipelineLayout;
}

VkPipelineLayout PipelineDataObject::getLayoutShadow()
{
    return shadowPipelineData.bindlessPipelineLayout;
}

void PipelineDataObject::createPipelineLayoutShadow()
{
    createPipelineLayoutForPipeline(&shadowPipelineData);
}
void PipelineDataObject::createPipelineLayoutOpaque()
{
    createPipelineLayoutForPipeline(&opaquePipelineData);
}

void PipelineDataObject::createPipelineLayoutForPipeline(perPipelineData* perPipelineData)
{

    if(perPipelineData->pipelineLayoutInitialized)
    {
        return;
    }
    
    std::vector<VkDescriptorSetLayout> layouts ={};
    if (perPipelineData->uniformDescriptorLayout) layouts.push_back(perPipelineData->uniformDescriptorLayout);
    if (perPipelineData->storageDescriptorLayout) layouts.push_back(perPipelineData->storageDescriptorLayout);
    if (perPipelineData->imageDescriptorLayout) layouts.push_back(perPipelineData->imageDescriptorLayout);
    if (perPipelineData->samplerDescriptorLayout) layouts.push_back(perPipelineData->samplerDescriptorLayout);
    
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = layouts.size(); // Optional
    pipelineLayoutInfo.pSetLayouts = layouts.data();

    //setup push constants
    VkPushConstantRange push_constant;
    //this push constant range starts at the beginning
    push_constant.offset = 0;
    //this push constant range takes up the size of a MeshPushConstants struct
    push_constant.size = sizeof(per_object_data);
    
    push_constant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    pipelineLayoutInfo.pPushConstantRanges = &push_constant;
    pipelineLayoutInfo.pushConstantRangeCount = 1;

    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &perPipelineData->bindlessPipelineLayout) != VK_SUCCESS)
    {
        printf("failed to create pipeline layout!");
    exit(-1);
    }
    perPipelineData->pipelineLayoutInitialized = true;
}

void PipelineDataObject::createGraphicsPipeline(std::vector<VkPipelineShaderStageCreateInfo> shaders, VkFormat* swapchainFormat, VkFormat* depthFormat, bool shadow, bool color, bool depth, bool lines)
{

    auto pipeline = shadow ? &shadowPipelineData : &opaquePipelineData;
    createPipelineLayoutForPipeline(pipeline);
    
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
    inputAssembly.topology = lines ? VK_PRIMITIVE_TOPOLOGY_LINE_LIST : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
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
    rasterizer.cullMode = shadow ? VK_CULL_MODE_FRONT_BIT : VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = shadow ? VK_TRUE : VK_FALSE;
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
    if (shadow)
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
        .colorAttachmentCount = (uint32_t)(color ? 1 : 0),
        .pColorAttachmentFormats = color ? swapchainFormat : VK_NULL_HANDLE ,
        .depthAttachmentFormat =  (VkFormat) (depth ? *depthFormat : 0)
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
    bindlesspipelineObjects.push_back(newGraphicsPipeline);
}
   


void PipelineDataObject::cleanup()
{
    
   
    for(int i = 0; i < bindlesspipelineObjects.size(); i++)
    {
        vkDestroyPipeline(device, bindlesspipelineObjects[i], nullptr);
    }

    opaquePipelineData.cleanup(device);
    shadowPipelineData.cleanup(device);
    
 
}

VkDescriptorSet PipelineDataObject::perPipelineData::getSetFromType(VkDescriptorType type,
    int currentFrame)
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

void PipelineDataObject::perPipelineData::cleanup(VkDevice device)
{
    vkDestroyPipelineLayout(device, bindlessPipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, uniformDescriptorLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, imageDescriptorLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, samplerDescriptorLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, storageDescriptorLayout, nullptr);
}
