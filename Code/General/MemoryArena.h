#pragma once
#include <cassert>
#include <memory>
#include <span>

//Simple arena allocator 
//Created with initialize, virtualfreed in destructor
//Usually used via "AllocSpan" (and related functions) to get a span of new memory
namespace MemoryArena
{
    struct Allocator
    {
        ptrdiff_t head = 0;
        ptrdiff_t last = 0;
        ptrdiff_t cursor = -1;
        ptrdiff_t size = 0;
        void* base;
        ~Allocator();
    };

    void Initialize(Allocator* m, uint32_t size = 100000);


    void* allocbytes(Allocator* a, ptrdiff_t allocSize, ptrdiff_t allocAlignment = 16);
	void* alloccopybytes(Allocator* a, void* ptr, size_t size_in_bytes);
	
	void FreeZeroInitialize(Allocator* a);
    void Free(Allocator* a);

    template <typename T>
    T* Alloc(Allocator* a, size_t ct = 1)
    {
         if (!(std::is_trivially_copyable<T>::value && std::is_standard_layout<T>::value))
         {
             assert(!"Use AllocDefaultConstructor for non-pod objects");
         }
        assert(!std::is_void< T >());
        T* ret = static_cast<T*>(allocbytes(a, ct * sizeof(T)));
        return ret;
    }

	template <typename  T, typename... Args>
    T* AllocConstruct(Allocator* a, Args&&... args)
    {
        assert(!std::is_void< T >());

		T* ret = static_cast<T*>(allocbytes(a, sizeof(T)));

        new (ret) T(std::forward<Args>(args)...);
        
        return ret;
    }

    template <typename T>
    T* AllocDefaultConstructor(Allocator* a, size_t ct = 1)
    {
      
        assert(!std::is_void< T >());
        auto ret = static_cast<T*>(allocbytes(a, ct * sizeof(T)));
        new (ret) T(); //Placement new 
        return ret;
    }


    template <typename T>
    T* AllocCopy(Allocator* a, T source)
    {
        assert(!std::is_void< T >());
        T* ret = static_cast<T*>(allocbytes(a, sizeof(T)));
        *ret = source;
        return ret;
    }

    ptrdiff_t GetCurrentOffset(Allocator* a);
    void FreeToOffset(Allocator* a, ptrdiff_t cursor);

    template <typename T>
    std::span<T> AllocSpan(Allocator* a, size_t length = 1)
    {
         if (!(std::is_trivially_copyable<T>::value && std::is_standard_layout<T>::value))
         {
             assert(!"Shouldn't allocspan non-pod objects -- did you mean to call AllocSpanDefaultConstructor?");
         }
        assert(!std::is_void< T >());
        auto size = length * sizeof(T);
        auto* start = static_cast<T*>(allocbytes(a, size, 16));
        std::span<T> ret{start, length};
        return ret;
    }

    template <typename T>
    std::span<T> AllocSpanDefaultConstructor(Allocator* a, size_t length = 1)
    {
        assert(!std::is_void< T >());

        auto size = length * sizeof(T);
        auto* start = static_cast<T*>(allocbytes(a, size, 16));
        std::span<T> ret{start, length};

        for(int i =0; i < ret.size(); i++)
        {
            new (&ret[i]) T();
        }
        
        return ret;
    }

    template <typename  T, typename... Args>
    std::span<T> AllocSpanConstructEntries(Allocator* a, size_t length, Args&&... args)
    {
        assert(!std::is_void< T >());

        auto size = length * sizeof(T);
        auto* start = static_cast<T*>(allocbytes(a, size, 16));
        std::span<T> ret{start, length};

        for(int i =0; i < ret.size(); i++)
        {
            new (&ret[i]) T(std::forward<Args>(args)...);
        }
        
        return ret;
    }


    template <typename T>
    std::span<T> AllocCopySpan(Allocator* a, std::span<T> src, size_t additionalLength = 0)
    {
		 if (!(std::is_trivially_copyable<T>::value && std::is_standard_layout<T>::value))
         {
             assert(!"Shouldn't copy non-pod objects");
         }
        assert(!std::is_void_v< T >);
        auto size = src.size() + additionalLength;
		auto dest = AllocSpan<T>(a, size);
		std::copy(src.data(), src.data() + src.size(), dest.data());
        return dest;
    }
}
typedef MemoryArena::Allocator* ArenaAllocator;