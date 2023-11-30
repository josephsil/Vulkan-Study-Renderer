#pragma once
#include <cassert>
#include <memory>
#include <span>

#include "forward-declarations-renderer.h"
#include "MeshLibraryImplementations.h"
#include <windows.h>

namespace VulkanMemory
{
    VmaAllocator GetAllocator(VkDevice device, VkPhysicalDevice
    physicalDevice, VkInstance instance);

    
    void DestroyBuffer(VmaAllocator allocator, VkBuffer buffer, VmaAllocation allocation);
    void DestroyImage(VmaAllocator allocator, VkImage image, VmaAllocation allocation);
    void MapMemory(VmaAllocator allocator,  VmaAllocation allocation, void** data);
    void UnmapMemory(VmaAllocator allocator,  VmaAllocation allocation);
    
}

typedef  ptrdiff_t aOFFSET;
namespace MemoryArena
{
    struct memoryArena
    {
        ptrdiff_t head;
        ptrdiff_t last;
        ptrdiff_t size;
        void* base;
        ~memoryArena()
        {
            VirtualFree(base, 0,MEM_RELEASE);
        }
    };

    void initialize(memoryArena* m, uint32_t size = 100000);


    void* alloc(memoryArena* a, ptrdiff_t allocSize, ptrdiff_t allocAlignment = 16);
    void free(memoryArena* a);
    void freeLast(memoryArena* a);

    template<typename T> T*get(memoryArena* a, aOFFSET offset)
    {
        assert(offset < a->head);
        return (T*)a->base + offset;
    }

    template<typename T> T *Alloc(memoryArena* a, size_t ct = 1) {
        T *ret = (T*)alloc(a, ct * sizeof(T));
        return ret;
    }

    template<typename T> std::span<T> AllocSpan(memoryArena* a, int length)
    {
        std::span<T> ret(alloc(a, length * sizeof(T)), length);
    }
    

    
}
