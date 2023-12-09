#pragma once
#include <vector>

#include "AppStruct.h"
#include "forward-declarations-renderer.h"
#include "SceneObjectData.h"


struct VkDescriptorSetLayoutCreateInfo;
struct VkDescriptorSetLayoutBinding;

struct layoutInfo
{
    VkDescriptorType type;
    uint32_t slot;
    uint32_t descriptorCount = 1;
};


struct LayoutsBuilder //What do you call the aggregate of several layouts?
{

    //TODO JS: Connect the two kinds of builders, so we like "fill slots" in the result of this one, and validate type/size?
    struct bindingBuilder
    {
        int i = 0;
        std::vector<VkDescriptorSetLayoutBinding> data;

        bindingBuilder(int size);

        void addBinding(VkDescriptorType type, VkShaderStageFlags stageflags, uint32_t count = 1);
    };

    
    bindingBuilder uniformbuilder = {0};
    bindingBuilder samplerbuilder = {0};
    bindingBuilder imagebuilder = {0};
    bindingBuilder storagebuilder = {0};
    
    std::vector<layoutInfo> bindings;

    LayoutsBuilder(int uniformCt, int imageCt, int storageCt);

    void addBinding(VkDescriptorType type, VkShaderStageFlags stage, uint32_t descriptorCount = 1);

    VkDescriptorSetLayoutCreateInfo getCreateInfo(VkDescriptorType type);
};

LayoutsBuilder createBindlessLayoutBuilder(
    RendererHandles rendererHandles, Scene* scene);

LayoutsBuilder createShadowLayoutBuilder(
    RendererHandles rendererHandles, Scene* scene);