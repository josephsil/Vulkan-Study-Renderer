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
        ptrdiff_t head = 0;
        ptrdiff_t last = 0;
        ptrdiff_t cursor = -1;
        ptrdiff_t size = 0;
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

    void* copy(memoryArena* a, void* ptr, size_t size_in_bytes);
    template<typename T> T *Alloc(memoryArena* a, size_t ct = 1) {
        T *ret = (T*)alloc(a, ct * sizeof(T));
        return ret;
    }

    void setCursor(memoryArena* a);
    void freeToCursor(memoryArena* a);

    template<typename T> std::span<T> AllocSpan(memoryArena* a, size_t length = 1)
    {
        T* start = (T*)alloc(a, length * sizeof(T), 16);
        std::span<T> ret {start, length};
        return ret;
    }
    

    template<typename T> std::span<T> copySpan(memoryArena* a, std::span<T> src)
    {
        T* start = (T*)MemoryArena::copy(a, src.data(), src.size_bytes());
        std::span<T> ret {start, src.size()};
        return ret;
    }


     

    

    
}
