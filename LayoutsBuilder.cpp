#include "LayoutsBuilder.h"
#include "AppStruct.h"
#include <vector>


#include "Vulkan_Includes.h"

LayoutsBuilder::bindingBuilder::bindingBuilder(int size)
{
    data.resize(size);
}

void LayoutsBuilder::bindingBuilder::addBinding(VkDescriptorType type, VkShaderStageFlags stageflags, uint32_t count)
{
    VkDescriptorSetLayoutBinding binding{};
    binding.binding = i; //b0
    binding.descriptorCount = count;
    binding.descriptorType = type;
    binding.stageFlags = stageflags;

    data[i] = binding;
    i++;
}

LayoutsBuilder::LayoutsBuilder(int uniformCt, int imageCt, int storageCt)
{
    uniformbuilder = bindingBuilder(uniformCt);
    samplerbuilder = bindingBuilder(imageCt);
    imagebuilder = bindingBuilder(imageCt);
    storagebuilder = bindingBuilder(storageCt);
    bindings = {};
}

void LayoutsBuilder::addBinding(VkDescriptorType type, VkShaderStageFlags stage, uint32_t descriptorCount)
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

VkDescriptorSetLayoutCreateInfo LayoutsBuilder::getCreateInfo(VkDescriptorType type)
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

LayoutsBuilder createBindlessLayoutBuilder(
    RendererHandles rendererHandles, Scene* scene)
{
    auto builder =LayoutsBuilder(1, 3, 3);
  
    builder.addBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL );
  
    builder.addBinding(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_SHADER_STAGE_FRAGMENT_BIT,  scene->materialTextureCount()  + scene->utilityTextureCount());
    builder.addBinding(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_SHADER_STAGE_FRAGMENT_BIT,  2);

    builder.addBinding(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_SHADER_STAGE_FRAGMENT_BIT,  MAX_SHADOWMAPS); //SHADOW
    
    builder.addBinding(VK_DESCRIPTOR_TYPE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT , scene->materialTextureCount()  + scene->utilityTextureCount());
    builder.addBinding(VK_DESCRIPTOR_TYPE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT , 2);
    builder.addBinding(VK_DESCRIPTOR_TYPE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT , 1); //SHADOW
    
    builder.addBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
    builder.addBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT); //light
    builder.addBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT); //ubo
    return builder;
}

LayoutsBuilder createShadowLayoutBuilder(
    RendererHandles rendererHandles, Scene* scene)
{
    auto builder =LayoutsBuilder(1, 0, 4);
  
    builder.addBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT );
  
    builder.addBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
    builder.addBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT); //light
    builder.addBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT); //ubo
    builder.addBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT); //light matrices
    return builder;
}