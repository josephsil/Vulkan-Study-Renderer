#pragma once
#include <cassert>
#include <memory>
#include <span>
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
        ~memoryArena();
    };

    void initialize(memoryArena* m, uint32_t size = 100000);


    void* alloc(memoryArena* a, ptrdiff_t allocSize, ptrdiff_t allocAlignment = 16);
    void free(memoryArena* a);
    void freeLast(memoryArena* a);
    void RELEASE(memoryArena* a);

    template<typename T> T*get(memoryArena* a, aOFFSET offset)
    {
        assert(!std::is_void_v<T>);
        assert(offset < a->head);
        return (T*)a->base + offset;
    }

    void* copy(memoryArena* a, void* ptr, size_t size_in_bytes);
    template<typename T> T *Alloc(memoryArena* a, size_t ct = 1) {
        assert(!std::is_void<T>());
        T *ret = (T*)alloc(a, ct * sizeof(T)); 
        return ret;
    }

    template<typename T> T *AllocCopy(memoryArena* a, T source) {
        assert(!std::is_void<T>());
        T *ret = (T*)alloc(a, sizeof(T));
        *ret = source;
        return ret;
    }

    void setCursor(memoryArena* a);
    void freeToCursor(memoryArena* a);

    template<typename T> std::span<T> AllocSpan(memoryArena* a, size_t length = 1)
    {
        assert(!std::is_void<T>());
        T* start = (T*)alloc(a, length * sizeof(T), 16);
        std::span<T> ret {start, length};
        return ret;
    }
    

    template<typename T> std::span<T> copySpan(memoryArena* a, std::span<T> src)
    {
        assert(!std::is_void_v<T>);
        T* start = (T*)MemoryArena::copy(a, src.data(), src.size_bytes());
        std::span<T> ret {start, src.size()};
        return ret;
    }


     

    

    
}
