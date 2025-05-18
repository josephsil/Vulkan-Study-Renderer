#include "RendererDeletionQueue.h"

#include <cassert>
#include <ktx.h>
#include <Renderer/VulkanIncludes/Vulkan_Includes.h>

#include <Renderer/VulkanIncludes/VulkanMemory.h>

#include "CommandPoolManager.h"

bool deleteableResource::operator==(const deleteableResource& other) const
{
    return (handle == other.handle);
}

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

    deleteableResource deletionObj = {t, vulkanObject};
    if (std::find(ToDeleteStaging.begin(), ToDeleteStaging.end(), deletionObj) != ToDeleteStaging.end())
    {
        auto minlength = std::min(20, (int)ToDeleteStaging.size());
        std::span<deleteableResource> recentResource =   std::span<deleteableResource>(ToDeleteStaging.data() + (ToDeleteStaging.size() - minlength), minlength);
        for (auto recent_resource : recentResource)
        {
            printf(">>> %llu \n", recent_resource.handle);
        }
        assert(!"duplicate deletionqueue push");
        return;
        
    }
    ToDeleteStaging.push_back(deletionObj);
    
}


void RendererDeletionQueue::push_backVMA(deletionType t, uint64_t vulkanObject, VmaAllocation allocation)
{
    assert(initialized != 0);
    assert(t == deletionType::vmaBuffer || t == deletionType::VmaImage);

    deleteableResource deletionObj = {t, vulkanObject, allocation};
    if (std::find(ToDeleteStaging.begin(), ToDeleteStaging.end(), deletionObj) != ToDeleteStaging.end())
    {
        auto minlength = std::min(20, (int)ToDeleteStaging.size());
        std::span<deleteableResource> recentResource =   std::span<deleteableResource>(ToDeleteStaging.data() + (ToDeleteStaging.size() - minlength), minlength);
        for (auto recent_resource : recentResource)
        {
            printf(">>> %llu \n", recent_resource.handle);
        }
        assert(!"duplicate deletionqueue push");
        
    }
    ToDeleteStaging.push_back(deletionObj);
    
}

void RendererDeletionQueue::TransferOwnershipTo(RendererDeletionQueue* target)
{
    for (auto element : ToDeleteStaging)
    {
        target->ToDeleteStaging.push_back(element);
    }
    ToDeleteStaging.clear();
}

void RendererDeletionQueue::FreeResource(VkDevice device, VmaAllocator allocator, deleteableResource r)
{
    
    deleteableResource resource = r;
    // printf("deleting %d %p\n", i,resource.handle);
    switch (resource.type)
    {
    case deletionType::KTXDestroyTexture:
    {
            ktxTexture_Destroy((ktxTexture*)(resource.handle));
        break;
    }
    case deletionType::vmaBuffer:
        {
            VulkanMemory::DestroyBuffer(allocator, (VkBuffer)(resource.handle), resource.vmaAllocation);
            break;
        }
    case deletionType::vkBuffer:
        {
            vkDestroyBuffer(device, (VkBuffer)(resource.handle), nullptr);
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
    case deletionType::CommandBuffer:
        {
            auto p = (CommandBufferPoolQueue)(resource.handle); 
            vkFreeCommandBuffers(device, p->pool, 1, &p->buffer);
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
    case deletionType::vkImage:
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
void RendererDeletionQueue::FreeQueue()
{


    assert(allocator != VK_NULL_HANDLE);
    std::vector<deleteableResource> toDelete = {};
    for (size_t i =1; i <= ToDeleteStaging.size(); i++)
    {
        size_t idx =  ToDeleteStaging.size() - i;
        auto to_delete = ToDeleteStaging[idx];

            if (std::find(toDelete.begin(), toDelete.end(), to_delete) != toDelete.end())
            {
                printf("SKIPPING DUPLICATE DELETION QUEUE ENTRY %llu \n", to_delete.handle);
                continue;
            }

        toDelete.push_back(to_delete);
    }
   
    for (auto r : toDelete)
    {
    FreeResource(device, allocator, r);
    }
    ToDeleteStaging.clear();
    // deletionList.clear();
}
