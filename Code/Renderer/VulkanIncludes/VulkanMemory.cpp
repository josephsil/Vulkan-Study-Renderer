#include "vmaImplementation.h"


#include "VulkanMemory.h"


VmaAllocator VulkanMemory::GetAllocator(VkDevice device, VkPhysicalDevice
                          physicalDevice, VkInstance instance)
{
    VmaAllocatorCreateInfo vmacreateInfo = {};
    vmacreateInfo.device = device;
    vmacreateInfo.physicalDevice = physicalDevice;
    vmacreateInfo.instance = instance;

    vmacreateInfo.vulkanApiVersion = VK_API_VERSION_1_3; 

    VmaAllocator allocator = {};
    vmaCreateAllocator(&vmacreateInfo, &allocator);

    return allocator;
}


void VulkanMemory::DestroyBuffer(VmaAllocator allocator, VkBuffer buffer, VmaAllocation allocation)
{
    vmaDestroyBuffer(allocator, buffer, allocation);
}
void VulkanMemory::DestroyImage(VmaAllocator allocator, VkImage image, VmaAllocation allocation)
{
    vmaDestroyImage(allocator, image, allocation);
}

void VulkanMemory::MapMemory(VmaAllocator allocator,  VmaAllocation allocation, void** data)
{
    vmaMapMemory(allocator,allocation, data);
}

void VulkanMemory::UnmapMemory(VmaAllocator allocator,  VmaAllocation allocation)
{
    vmaUnmapMemory(allocator, allocation);
}
