
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
        _createInfo.flags =   VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
        _createInfo.bindingCount = static_cast<uint32_t>(bindings->size());
        _createInfo.pBindings = bindings->data();

        return _createInfo;
    }

    DescriptorSets::bindlessDescriptorSetData createLayouts(VkDevice device)
    {
        VkDescriptorSetLayoutCreateInfo uniformDescriptorLayout =  this->getCreateInfo(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        VkDescriptorSetLayoutCreateInfo imageDescriptorLayout = this->getCreateInfo(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
        VkDescriptorSetLayoutCreateInfo samplerDescriptorLayout = this->getCreateInfo(VK_DESCRIPTOR_TYPE_SAMPLER);
        VkDescriptorSetLayoutCreateInfo storageDescriptorLayout = this->getCreateInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

        DescriptorSets::bindlessDescriptorSetData layout = {};
        VK_CHECK(vkCreateDescriptorSetLayout(device, &uniformDescriptorLayout, nullptr, &layout.uniformDescriptorLayout));
        VK_CHECK(vkCreateDescriptorSetLayout(device, &imageDescriptorLayout, nullptr, &layout.imageDescriptorLayout));
        VK_CHECK(vkCreateDescriptorSetLayout(device, &samplerDescriptorLayout, nullptr, &layout.samplerDescriptorLayout));
        VK_CHECK(vkCreateDescriptorSetLayout(device, &storageDescriptorLayout, nullptr, &layout.storageDescriptorLayout));
        layout.slots = this->bindings;

        return layout;
    }
    
};
void DescriptorSets::bindlessDescriptorSetData::bindToCommandBuffer(VkCommandBuffer cmd, VkPipelineLayout layout, uint32_t currentFrame)
{
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &uniformDescriptorSetForFrame[currentFrame], 0, nullptr);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 1, 1, &storageDescriptorSetForFrame[currentFrame], 0, nullptr);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 2, 1, &imageDescriptorSetForFrame[currentFrame], 0, nullptr);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 3, 1, &samplerDescriptorSetForFrame[currentFrame], 0, nullptr);
    
}
//updates descriptor sets based on vector of descriptorupdatedata, with some light validation that we're binding the right stuff
void DescriptorSets::bindlessDescriptorSetData::updateDescriptorSets(VkDevice device, std::vector<descriptorUpdateData> descriptorUpdates, uint32_t currentFrame)
{
    writeDescriptorSets.clear();
    writeDescriptorSetsBindingIndices.clear();
    
    for(int i = 0; i < descriptorUpdates.size(); i++)
    {
        descriptorUpdateData update = descriptorUpdates[i];
        VkDescriptorSet set = getSetFromType(descriptorUpdates[i].type, currentFrame);
        
        if (!writeDescriptorSetsBindingIndices.contains(set))
        {
            writeDescriptorSetsBindingIndices[set] = 0;
        }
        
        layoutInfo info = slots[writeDescriptorSets.size()];

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
    
    assert(writeDescriptorSets.size() == slots.size());
    
    vkUpdateDescriptorSets(device, writeDescriptorSets.size(), writeDescriptorSets.data(), 0, nullptr);
    
}

void DescriptorSets::bindlessDescriptorSetData::createDescriptorSets(RendererHandles handles,
                                                                     VkDescriptorPool pool, int MAX_FRAMES_IN_FLIGHT)
{
    assert(!descriptorSetsInitialized); // Don't double initialize
    uniformDescriptorSetForFrame.resize(MAX_FRAMES_IN_FLIGHT);
    imageDescriptorSetForFrame.resize(MAX_FRAMES_IN_FLIGHT);
    samplerDescriptorSetForFrame.resize(MAX_FRAMES_IN_FLIGHT);
    storageDescriptorSetForFrame.resize(MAX_FRAMES_IN_FLIGHT);
    for(int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        AllocateDescriptorSet(handles, pool,  &uniformDescriptorLayout, &uniformDescriptorSetForFrame[i]);
        AllocateDescriptorSet(handles, pool,  &imageDescriptorLayout, &imageDescriptorSetForFrame[i]);
        AllocateDescriptorSet(handles, pool,  &samplerDescriptorLayout, &samplerDescriptorSetForFrame[i]);
        AllocateDescriptorSet(handles, pool,  &storageDescriptorLayout, &storageDescriptorSetForFrame[i]);
    }
    descriptorSetsInitialized = true;
            
}

DescriptorSets::bindlessDescriptorSetData DescriptorSets::createBindlessLayout(
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
    return builder.createLayouts(rendererHandles.device);
}

//TODO JS: VK_KHR_dynamic_rendering gets rid of render pass types and just lets you vkBeginRenderPass
void RenderingSetup::createRenderPass(RendererHandles rendererHandles, RenderTextureFormat passformat, VkRenderPass* pass)
{
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = passformat.ColorFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = passformat.DepthFormat;
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(rendererHandles.device, &renderPassInfo, nullptr, pass) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create render pass!");
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

void DescriptorSets::AllocateDescriptorSet(RendererHandles handles, VkDescriptorPool pool, VkDescriptorSetLayout* pdescriptorsetLayout, VkDescriptorSet* pset)
{
    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.pNext = nullptr;
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = pool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = pdescriptorsetLayout;
    VK_CHECK(vkAllocateDescriptorSets(handles.device, &allocInfo, pset));
  
}

VkDescriptorBufferInfo dataBuffer::getBufferInfo()
{
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = data; //TODO: For loop over frames in flight
    bufferInfo.offset = 0;
    bufferInfo.range = size;
    return bufferInfo;
}
