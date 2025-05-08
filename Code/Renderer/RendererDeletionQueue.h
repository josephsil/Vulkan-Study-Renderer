#pragma once
#include <vector>
#include "VulkanIncludes/forward-declarations-renderer.h"

enum class deletionType
{
    vmaBuffer,
    descriptorPool,
    Semaphore,
    Fence,
    CommandPool,
    ImageView,
    Image,
    VkMemory,
    VmaImage,
    Sampler,
    CommandBuffer
};

struct deleteableResource
{
    deletionType type;
    uint64_t handle;
    VmaAllocation vmaAllocation; // usually not defined 
};

class RendererDeletionQueue
{
public: 
    RendererDeletionQueue(VkDevice d, VmaAllocator vmaAllocator);
    void push_backVk(deletionType t, uint64_t vulkanObject);
    void push_backVMA(deletionType t, uint64_t vulkanObject, VmaAllocation allocation);
    void FreeQueue();

private:
    std::vector<deleteableResource> deletionList;
    VkDevice device;
    VmaAllocator allocator;
    int initialized = 0;
};
