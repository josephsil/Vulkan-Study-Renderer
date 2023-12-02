#include "vmaImplementation.h"

#include "Memory.h"

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

void MemoryArena::initialize(memoryArena* arena, uint32_t size)
{
    arena->base = VirtualAlloc(0, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    arena->head = 0;
    arena->size = size;
}

void* MemoryArena::alloc(memoryArena* a, ptrdiff_t allocSize, ptrdiff_t allocAlignment)
{
    void* headPtr = (char *)a->base + a->head;
    size_t sizeLeft = a->size - a->head;
    void* res = std::align(allocAlignment, allocSize, headPtr, sizeLeft);
    assert(res);
    a->last = a->head;
    a->head = ((char *)res + allocSize) - (char *)a->base; //bleh
    return res;
}

void MemoryArena::free(memoryArena* a)
{
    a->head = 0;
    a->last = 0;
}

void* MemoryArena::copy(memoryArena* a, void* ptr, size_t size_in_bytes)
{
    void* tgt = MemoryArena::alloc(a, size_in_bytes);
    memcpy(tgt, ptr, size_in_bytes);
    return tgt;
}

void MemoryArena::setCursor(memoryArena* a)
{
    a->cursor = a->head;
}

void MemoryArena::freeToCursor(memoryArena* a)
{
    assert(a->cursor <= 0 && a->cursor < a->head);
    a->head = a->cursor;
}


void MemoryArena::freeLast(memoryArena* a)
{
    a->head = a->last;   
}
