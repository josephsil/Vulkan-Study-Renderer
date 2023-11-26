
#include "vulkan-utilities.h"

#include <array>
#include <iostream>
#include <unordered_map>

#include "AppStruct.h"
#include "CommandPoolManager.h"
#include "TextureData.h"
#include "Vulkan_Includes.h"
#include "SceneObjectData.h"


//TODO JS: Connect the two kinds of builders, so we like "fill slots" in the result of this one, and validate type/size?
struct bindingBuilder
{
    int i = 0;
   std::vector<VkDescriptorSetLayoutBinding> data;

    bindingBuilder(int size)
    {
        data.resize(size);
    }
    void addBinding(VkDescriptorType type, VkShaderStageFlags stageflags, uint32_t count = 1)
    {
        VkDescriptorSetLayoutBinding binding{};
        binding.binding = i; //b0
        binding.descriptorCount = count;
        binding.descriptorType = type;
        binding.stageFlags = stageflags;

        data[i] = binding;
        i++;
    }
};

struct layoutsBuilder //What do you call the aggregate of several layouts?
{
    bindingBuilder uniformbuilder = {0};
    bindingBuilder samplerbuilder = {0};
    bindingBuilder imagebuilder = {0};
    bindingBuilder storagebuilder = {0};
    
    std::vector<DescriptorSets::layoutInfo> bindings;

    layoutsBuilder(int uniformCt, int imageCt, int storageCt)
    {
        uniformbuilder = bindingBuilder(uniformCt);
        samplerbuilder = bindingBuilder(imageCt);
        imagebuilder = bindingBuilder(imageCt);
        storagebuilder = bindingBuilder(storageCt);
        bindings = {};
    }

    void addBinding(VkDescriptorType type, VkShaderStageFlags stage, uint32_t descriptorCount = 1)
    {
        uint32_t ct;
        switch (type)
        {
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
            ct = uniformbuilder.i;
            uniformbuilder.addBinding(type, stage, descriptorCount);
            break;
        case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
            ct = samplerbuilder.i;
            samplerbuilder.addBinding(type, stage, descriptorCount);
            break;
        case VK_DESCRIPTOR_TYPE_SAMPLER:
            ct = imagebuilder.i;
            imagebuilder.addBinding(type, stage, descriptorCount);
            break;
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
            ct = storagebuilder.i;
            storagebuilder.addBinding(type, stage, descriptorCount);
            break;
        default:
            exit(-1);
        }

        bindings.push_back({type, ct, descriptorCount});
      
    }
    VkDescriptorSetLayoutCreateInfo getCreateInfo(VkDescriptorType type)
    {
        
        std::vector<VkDescriptorSetLayoutBinding>* bindings;
        bindings = &uniformbuilder.data;
        switch (type)
        {
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
             bindings = &uniformbuilder.data;
            break;
        case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
             bindings = &samplerbuilder.data;
            break;
        case VK_DESCRIPTOR_TYPE_SAMPLER:
            bindings =  &imagebuilder.data;
            break;
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
             bindings = &storagebuilder.data;
            break;

        }

        VkDescriptorSetLayoutCreateInfo _createInfo = {};
        _createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        // _createInfo.flags =   VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
        _createInfo.bindingCount = static_cast<uint32_t>(bindings->size());
        _createInfo.pBindings = bindings->data();

        return _createInfo;
    }
    
    
};

layoutsBuilder createBindlessLayout(
    RendererHandles rendererHandles, Scene* scene)
{
    auto builder =layoutsBuilder(1, 2, 3);
  
    builder.addBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL );
  
    builder.addBinding(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_SHADER_STAGE_FRAGMENT_BIT,  scene->materialTextureCount()  + scene->utilityTextureCount());
    builder.addBinding(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_SHADER_STAGE_FRAGMENT_BIT,  2);
    
    builder.addBinding(VK_DESCRIPTOR_TYPE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT , scene->materialTextureCount()  + scene->utilityTextureCount());
    builder.addBinding(VK_DESCRIPTOR_TYPE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT , 2);
    
    builder.addBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
    builder.addBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
    builder.addBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
    return builder;
}


DescriptorSets::bindlessDrawData::bindlessDrawData(RendererHandles handles, Scene* pscene)
{
    device = handles.device;
    createOpaqueLayout(handles , pscene);
    createShadowLayout(handles , pscene);
}

void DescriptorSets::bindlessDrawData::createOpaqueLayout(RendererHandles handles, Scene* pscene)
{
    layoutsBuilder builder = createBindlessLayout(handles,  pscene);
    VkDescriptorSetLayoutCreateInfo uniformDescriptorLayout =  builder.getCreateInfo(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    VkDescriptorSetLayoutCreateInfo imageDescriptorLayout = builder.getCreateInfo(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
    VkDescriptorSetLayoutCreateInfo samplerDescriptorLayout = builder.getCreateInfo(VK_DESCRIPTOR_TYPE_SAMPLER);
    VkDescriptorSetLayoutCreateInfo storageDescriptorLayout = builder.getCreateInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

    VK_CHECK(vkCreateDescriptorSetLayout(handles.device, &uniformDescriptorLayout, nullptr, &this->opaquePipelineData.uniformDescriptorLayout));
    VK_CHECK(vkCreateDescriptorSetLayout(handles.device, &imageDescriptorLayout, nullptr, &this->opaquePipelineData.imageDescriptorLayout));
    VK_CHECK(vkCreateDescriptorSetLayout(handles.device, &samplerDescriptorLayout, nullptr, &this->opaquePipelineData.samplerDescriptorLayout));
    VK_CHECK(vkCreateDescriptorSetLayout(handles.device, &storageDescriptorLayout, nullptr, &this->opaquePipelineData.storageDescriptorLayout));
    this->opaquePipelineData.slots = builder.bindings;
}
void DescriptorSets::bindlessDrawData::createShadowLayout(RendererHandles handles, Scene* pscene)
{
    layoutsBuilder builder = createBindlessLayout(handles,  pscene); //TODO JS: different layout
    VkDescriptorSetLayoutCreateInfo uniformDescriptorLayout =  builder.getCreateInfo(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    VkDescriptorSetLayoutCreateInfo imageDescriptorLayout = builder.getCreateInfo(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
    VkDescriptorSetLayoutCreateInfo samplerDescriptorLayout = builder.getCreateInfo(VK_DESCRIPTOR_TYPE_SAMPLER);
    VkDescriptorSetLayoutCreateInfo storageDescriptorLayout = builder.getCreateInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

    VK_CHECK(vkCreateDescriptorSetLayout(handles.device, &uniformDescriptorLayout, nullptr, &this->shadowPipelineData.uniformDescriptorLayout));
    VK_CHECK(vkCreateDescriptorSetLayout(handles.device, &imageDescriptorLayout, nullptr, &this->shadowPipelineData.imageDescriptorLayout));
    VK_CHECK(vkCreateDescriptorSetLayout(handles.device, &samplerDescriptorLayout, nullptr, &this->shadowPipelineData.samplerDescriptorLayout));
    VK_CHECK(vkCreateDescriptorSetLayout(handles.device, &storageDescriptorLayout, nullptr, &this->shadowPipelineData.storageDescriptorLayout));
    this->shadowPipelineData.slots = builder.bindings;
}

void DescriptorSets::bindlessDrawData::bindToCommandBufferOpaque(VkCommandBuffer cmd, uint32_t currentFrame)
{
    bindToCommandBuffer(cmd, currentFrame, &opaquePipelineData);
}

void DescriptorSets::bindlessDrawData::bindToCommandBufferShadow(VkCommandBuffer cmd, uint32_t currentFrame)
{
    bindToCommandBuffer(cmd, currentFrame, &shadowPipelineData);
}
void DescriptorSets::bindlessDrawData::bindToCommandBuffer(VkCommandBuffer cmd, uint32_t currentFrame, perPipelineData* pipelinedata)
{
    assert(pipelinedata->descriptorSetsInitialized && pipelinedata->pipelinesInitialized);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelinedata->bindlessPipelineLayout, 0, 1, &pipelinedata->uniformDescriptorSetForFrame[currentFrame], 0, nullptr);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelinedata->bindlessPipelineLayout, 1, 1, &pipelinedata->storageDescriptorSetForFrame[currentFrame], 0, nullptr);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelinedata->bindlessPipelineLayout, 2, 1, &pipelinedata->imageDescriptorSetForFrame[currentFrame], 0, nullptr);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelinedata->bindlessPipelineLayout, 3, 1, &pipelinedata->samplerDescriptorSetForFrame[currentFrame], 0, nullptr);
    
}

void DescriptorSets::bindlessDrawData::updateDescriptorSetsForPipeline(std::vector<descriptorUpdateData> descriptorUpdates, uint32_t currentFrame, perPipelineData* perPipelineData)
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
void DescriptorSets::bindlessDrawData::updateOpaqueDescriptorSets(std::vector<descriptorUpdateData> descriptorUpdates, uint32_t currentFrame)
{
   updateDescriptorSetsForPipeline(descriptorUpdates, currentFrame, &opaquePipelineData);
}

void DescriptorSets::bindlessDrawData::updateShadowDescriptorSets(std::vector<descriptorUpdateData> descriptorUpdates, uint32_t currentFrame)
{
    updateDescriptorSetsForPipeline(descriptorUpdates, currentFrame, &shadowPipelineData);
}


void DescriptorSets::bindlessDrawData::createDescriptorSetsOpaque(VkDescriptorPool pool, int MAX_FRAMES_IN_FLIGHT)
{
    createDescriptorSetsforPipeline(pool, MAX_FRAMES_IN_FLIGHT, &opaquePipelineData);
}

void DescriptorSets::bindlessDrawData::createDescriptorSetsShadow(VkDescriptorPool pool, int MAX_FRAMES_IN_FLIGHT)
{
    createDescriptorSetsforPipeline(pool, MAX_FRAMES_IN_FLIGHT, &shadowPipelineData);
}
void DescriptorSets::bindlessDrawData::createDescriptorSetsforPipeline(VkDescriptorPool pool, int MAX_FRAMES_IN_FLIGHT, perPipelineData* perPipelineData)
{
    assert(!perPipelineData->descriptorSetsInitialized); // Don't double initialize
    perPipelineData->uniformDescriptorSetForFrame.resize(MAX_FRAMES_IN_FLIGHT);
    perPipelineData->imageDescriptorSetForFrame.resize(MAX_FRAMES_IN_FLIGHT);
    perPipelineData->samplerDescriptorSetForFrame.resize(MAX_FRAMES_IN_FLIGHT);
    perPipelineData->storageDescriptorSetForFrame.resize(MAX_FRAMES_IN_FLIGHT);
    for(int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        AllocateDescriptorSet(device, pool,  &perPipelineData->uniformDescriptorLayout, &perPipelineData->uniformDescriptorSetForFrame[i]);
        AllocateDescriptorSet(device, pool,  &perPipelineData->imageDescriptorLayout, &perPipelineData->imageDescriptorSetForFrame[i]);
        AllocateDescriptorSet(device, pool,  &perPipelineData->samplerDescriptorLayout, &perPipelineData->samplerDescriptorSetForFrame[i]);
        AllocateDescriptorSet(device, pool,  &perPipelineData->storageDescriptorLayout, &perPipelineData->storageDescriptorSetForFrame[i]);
    }
    perPipelineData->descriptorSetsInitialized = true;
            
}

VkPipeline DescriptorSets::bindlessDrawData::getPipeline(int index)
{
    //TODO JS
    // assert(pipelinesInitialized && pipelineLayoutInitialized);
    assert(index < bindlesspipelineObjects.size());
    return bindlesspipelineObjects[index];
}

VkPipelineLayout DescriptorSets::bindlessDrawData::getLayoutOpaque()
{
    return opaquePipelineData.bindlessPipelineLayout;
}

VkPipelineLayout DescriptorSets::bindlessDrawData::getLayoutShadow()
{
    return shadowPipelineData.bindlessPipelineLayout;
}

void DescriptorSets::bindlessDrawData::createPipelineLayoutShadow()
{
    createPipelineLayoutForPipeline(&shadowPipelineData);
}
void DescriptorSets::bindlessDrawData::createPipelineLayoutOpaque()
{
    createPipelineLayoutForPipeline(&opaquePipelineData);
}

void DescriptorSets::bindlessDrawData::createPipelineLayoutForPipeline(perPipelineData* perPipelineData)
{

    if(perPipelineData->pipelineLayoutInitialized)
    {
        return;
    }
    
    std::vector<VkDescriptorSetLayout> layouts = {
        perPipelineData->uniformDescriptorLayout,perPipelineData->storageDescriptorLayout,perPipelineData->imageDescriptorLayout,perPipelineData->samplerDescriptorLayout
    };
    
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

void DescriptorSets::bindlessDrawData::createGraphicsPipeline(std::vector<VkPipelineShaderStageCreateInfo> shaders, VkFormat* swapchainFormat, VkFormat* depthFormat, bool shadow, bool color, bool depth)
{

    auto pipeline = shadow ? &shadowPipelineData : &opaquePipelineData;
    createPipelineLayoutForPipeline(pipeline);
    
    assert(pipeline->pipelineLayoutInitialized);
    VkPipeline newGraphicsPipeline {}; 

    VkPipelineShaderStageCreateInfo shaderStages[] = {shaders[0], shaders[1]};

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 0;
    vertexInputInfo.vertexAttributeDescriptionCount = 0;
    vertexInputInfo.pVertexBindingDescriptions = nullptr;
    vertexInputInfo.pVertexAttributeDescriptions = nullptr;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
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
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.minSampleShading = 1.0f; // Optional
    multisampling.pSampleMask = nullptr; // Optional
    multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
    multisampling.alphaToOneEnable = VK_FALSE; // Optional

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD; // Optional
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD; // Optional

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.minDepthBounds = 0.0f; // Optional
    depthStencil.maxDepthBounds = 1.0f; // Optional
    depthStencil.stencilTestEnable = VK_FALSE;
    depthStencil.front = {}; // Optional
    depthStencil.back = {}; // Optional

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f; // Optional
    colorBlending.blendConstants[1] = 0.0f; // Optional
    colorBlending.blendConstants[2] = 0.0f; // Optional
    colorBlending.blendConstants[3] = 0.0f; // Optional

    std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();
    
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;

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
   


void DescriptorSets::bindlessDrawData::cleanup()
{
    
   
    for(int i = 0; i < bindlesspipelineObjects.size(); i++)
    {
        vkDestroyPipeline(device, bindlesspipelineObjects[i], nullptr);
    }

    opaquePipelineData.cleanup(device);
    shadowPipelineData.cleanup(device);
    
 
}

VkDescriptorSet DescriptorSets::bindlessDrawData::perPipelineData::getSetFromType(VkDescriptorType type,
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

VkFormat Capabilities::findDepthFormat(RendererHandles rendererHandles)
{
    return findSupportedFormat(rendererHandles,
                               {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
                               VK_IMAGE_TILING_OPTIMAL,
                               VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
    );
}


VkFormat Capabilities::findSupportedFormat(RendererHandles rendererHandles, const std::vector<VkFormat>& candidates,
                                           VkImageTiling tiling,
                                           VkFormatFeatureFlags features)
{
    for (VkFormat format : candidates)
    {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(rendererHandles.physicalDevice, format, &props);

        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features)
        {
            return format;
        }
        if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features)
        {
            return format;
        }
    }

    std::cerr << "failed to find supported format!" << "\n";
    exit(1);
}

uint32_t Capabilities::findMemoryType(RendererHandles rendererHandles, uint32_t typeFilter,
                                      VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(rendererHandles.physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
    {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
        {
            return i;
        }
    }

    throw std::runtime_error("failed to find suitable memory type!");
}


//This probably doesnt need to exisdt
std::pair<std::vector<VkDescriptorImageInfo>, std::vector<VkDescriptorImageInfo>>
DescriptorSets::ImageInfoFromImageDataVec(std::vector<TextureData> textures)
{
    std::vector<VkDescriptorImageInfo> imageinfos(textures.size());
    std::vector<VkDescriptorImageInfo> samplerinfos(textures.size());
    for (int i = 0; i < textures.size(); i++)
    {
        imageinfos[i] = VkDescriptorImageInfo{
            .imageView = textures[i].textureImageView, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };
        samplerinfos[i] = VkDescriptorImageInfo{
            .sampler = textures[i].textureSampler, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };
    }

    return std::make_pair(imageinfos, samplerinfos);
}

void DescriptorSets::AllocateDescriptorSet(VkDevice device, VkDescriptorPool pool, VkDescriptorSetLayout* pdescriptorsetLayout, VkDescriptorSet* pset)
{
    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.pNext = nullptr;
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = pool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = pdescriptorsetLayout;
    VkResult result = vkAllocateDescriptorSets(device, &allocInfo, pset);
    VK_CHECK(result);
  
}


VkDescriptorBufferInfo dataBuffer::getBufferInfo()
{
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = data; //TODO: For loop over frames in flight
    bufferInfo.offset = 0;
    bufferInfo.range = size;
    return bufferInfo;
}
