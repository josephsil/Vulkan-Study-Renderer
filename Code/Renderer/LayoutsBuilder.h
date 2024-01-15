#pragma once
#include <vector>

#include "RendererHandles.h"
#include "VulkanIncludes/forward-declarations-renderer.h"
// #include "SceneObjectData.h"

class Scene;

struct VkDescriptorSetLayoutCreateInfo;
struct VkDescriptorSetLayoutBinding;



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

    
    bindingBuilder perSceneBindingsLayout = {0};

    LayoutsBuilder(int uniformCt, int imageCt, int storageCt);

    void addBinding(VkDescriptorType type, VkShaderStageFlags stage, uint32_t descriptorCount = 1);

    VkDescriptorSetLayoutCreateInfo getCreateInfo(VkDescriptorType type);
};
