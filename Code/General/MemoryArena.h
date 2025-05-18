#pragma once
#include <cassert>
#include <memory>
#include <span>
using aOFFSET = ptrdiff_t;


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

    template <typename T>
    T* get(memoryArena* a, aOFFSET offset)
    {
        assert(!std::is_void_v< T >);
        assert(offset < a->head);
        return static_cast<T*>(a->base) + offset;
    }

    void* copy(memoryArena* a, void* ptr, size_t size_in_bytes);

    template <typename T>
    T* Alloc(memoryArena* a, size_t ct = 1)
    {
        // if (!(std::is_trivially_copyable<T>::value && std::is_standard_layout<T>::value))
        // {
        //     assert(!"Use ALlocDefaultInitialize for non-pod objects");
        // }
        assert(!std::is_void< T >());
        T* ret = static_cast<T*>(alloc(a, ct * sizeof(T)));
        return ret;
    }

    template <typename T>
    T* AllocDefaultInitialize(memoryArena* a, size_t ct = 1)
    {
      
        assert(!std::is_void< T >());
        auto ret = static_cast<T*>(alloc(a, ct * sizeof(T)));
        new (ret) T(); //Placement new 
        return ret;
    }


    template <typename T>
    T* AllocCopy(memoryArena* a, T source)
    {
        assert(!std::is_void< T >());
        T* ret = static_cast<T*>(alloc(a, sizeof(T)));
        *ret = source;
        return ret;
    }

    void setCursor(memoryArena* a);
    void freeToCursor(memoryArena* a);

    template <typename T>
    std::span<T> AllocSpan(memoryArena* a, size_t length = 1)
    {
        // if (!(std::is_trivially_copyable<T>::value && std::is_standard_layout<T>::value))
        // {
        //     assert(!"Shouldn't allocspan non-pod objects");
        // }
        assert(!std::is_void< T >());
        auto size = length * sizeof(T);
        auto* start = static_cast<T*>(alloc(a, size, 16));
        std::span<T> ret{start, length};
        return ret;
    }

    template <typename T>
    std::span<T> AllocSpanDefaultInitialize(memoryArena* a, size_t length = 1)
    {
        assert(!std::is_void< T >());

        auto size = length * sizeof(T);
        auto* start = static_cast<T*>(alloc(a, size, 16));
        std::span<T> ret{start, length};

        for(int i =0; i < ret.size(); i++)
        {
            new (&ret[i]) T();
        }
        
        return ret;
    }

    template <typename  T, typename... Args>
    std::span<T> AllocSpanEmplaceInitialize(memoryArena* a, size_t length, Args&&... args)
    {
        assert(!std::is_void< T >());

        auto size = length * sizeof(T);
        auto* start = static_cast<T*>(alloc(a, size, 16));
        std::span<T> ret{start, length};

        for(int i =0; i < ret.size(); i++)
        {
            new (&ret[i]) T(std::forward<Args>(args)...);
        }
        
        return ret;
    }

    template <typename  T, typename... Args>
std::span<T> AllocSpanEmplaceInitialize(memoryArena* a, uint32_t length, Args&&... args)
    {
        assert(!std::is_void< T >());

        auto size = length * sizeof(T);
        auto* start = static_cast<T*>(alloc(a, size, 16));
        std::span<T> ret{start, length};

        for(int i =0; i < ret.size(); i++)
        {
            new (&ret[i]) T(std::forward<Args>(args)...);
        }
        
        return ret;
    }


    template <typename T>
    std::span<T> copySpan(memoryArena* a, std::span<T> src, size_t additionalLength = 0)
    {
        assert(!std::is_void_v< T >);
        // assert(src.extent != std::dynamic_extent);
        auto size = src.size() + additionalLength;
        T* start = static_cast<T*>(MemoryArena::copy(a, src.data(), size * sizeof(T)));
        std::span<T> ret{start, src.size()};
        return ret;
    }
}
typedef MemoryArena::memoryArena* ArenaAllocator;