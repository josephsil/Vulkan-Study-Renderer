#pragma once
#include <concurrentqueue.h>
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
    CommandBuffer,
    KTXDestroyTexture,
    vkBuffer,
    vkImage,
};

struct deleteableResource
{
    deletionType type;
    uint64_t handle;
    VmaAllocation vmaAllocation; // usually not defined
    bool operator==(const deleteableResource& other) const;
};

class RendererDeletionQueue
{
public:
    static void FreeResource(VkDevice device, VmaAllocator allocator, deleteableResource r);
    void push_backVk(deletionType t, uint64_t vulkanObject);
    void push_backVMA(deletionType t, uint64_t vulkanObject, VmaAllocation allocation);
    void TransferOwnershipTo(RendererDeletionQueue* target);
    void FreeQueue();

	VkDevice device;
    VmaAllocator allocator;
    int initialized = 0;
    std::vector<deleteableResource> DeletionList;
   
};

void InitRendererDeletionQueue(RendererDeletionQueue* queue, VkDevice d, VmaAllocator vmaAllocator);