#include "RendererDeletionQueue.h"

#include <cassert>
#include "Renderer/VulkanIncludes/Vulkan_Includes.h"

#include "VulkanIncludes/VulkanMemory.h"

RendererDeletionQueue::RendererDeletionQueue(VkDevice d, VmaAllocator vmaAllocator)
{
    device = d;
    allocator = vmaAllocator;
    initialized = 1;
}

void RendererDeletionQueue::push_backVk(deletionType t, uint64_t vulkanObject)
{
    assert(initialized != 0);
    assert(t != deletionType::vmaBuffer && t != deletionType::VmaImage);
    deletionList.push_back({t, vulkanObject});
}


void RendererDeletionQueue::push_backVMA(deletionType t, uint64_t vulkanObject, VmaAllocation allocation)
{
    assert(initialized != 0);
    assert(t == deletionType::vmaBuffer || t == deletionType::VmaImage);
    deletionList.push_back({t, vulkanObject, allocation});
}

void RendererDeletionQueue::FreeQueue()
{
    for (int i = deletionList.size() - 1; i >= 0; i--)
    {
        deleteableResource resource = deletionList[i];
        // printf("deleting %d %p\n", i,resource.handle);
        switch (resource.type)
        {
        case deletionType::vmaBuffer:
            {
                VulkanMemory::DestroyBuffer(allocator, (VkBuffer)(resource.handle), resource.vmaAllocation);
                break;
            }
        case deletionType::descriptorPool:
            {
                vkDestroyDescriptorPool(device, (VkDescriptorPool)resource.handle, nullptr);
                break;
            }
        case deletionType::Semaphore:
            {
                vkDestroySemaphore(device, (VkSemaphore)resource.handle, nullptr);
                break;
            }
        case deletionType::Fence:
            {
                vkDestroyFence(device, (VkFence)(resource.handle), nullptr);
                break;
            }
        case deletionType::CommandPool:
            {
                vkDestroyCommandPool(device, (VkCommandPool)(resource.handle), nullptr);
                break;
            }
        case deletionType::ImageView:
            {
                vkDestroyImageView(device, (VkImageView)(resource.handle), nullptr);
                break;
            }
        case deletionType::Image:
            {
                vkDestroyImage(device, (VkImage)(resource.handle), nullptr);
                break;
            }
        case deletionType::VkMemory:
            {
                vkFreeMemory(device, (VkDeviceMemory)(resource.handle), nullptr);
                break;
            }
        case deletionType::VmaImage:
            {
                VulkanMemory::DestroyImage(allocator, (VkImage)(resource.handle), resource.vmaAllocation);
                break;
            }
        case deletionType::Sampler:
            {
                vkDestroySampler(device, (VkSampler)(resource.handle), nullptr);
                break;
            }
        }
    }
    deletionList.clear();
}
