#pragma once
#include <cassert>
#include <span>
#include <General/MemoryArena.h>
#include <General/Array.h>

template<class T,class U>
inline void CopySpanRemappedSRC(std::span<T> src, std::span<T> dst, std::span<U> srcIndices)
{
 static_assert( std::is_unsigned_v<U>, "U must be an unsigned type" );
    for (size_t i = 0; i < srcIndices.size(); i++)
    {
        dst[i] = src[srcIndices[i]];
    }
    
}

template<class T>
inline void FillIndicesAscendingNoAlloc(std::span<T> numbers)
{
    static_assert( std::is_unsigned_v<T>, "U must be an unsigned type" );
    for (T i = 0; i < numbers.size(); i++)
    {
        numbers[i] = i;
    }
}

template<class T>
inline std::span<T> GetIndicesAscending(ArenaAllocator allocator, size_t size)
{
    static_assert( std::is_unsigned_v<T>, "U must be an unsigned type" );
	std::span<T> indices = MemoryArena::AllocSpan<size_t>(allocator, size);
	FillIndicesAscendingNoAlloc(indices);
	return indices;
}


//
//sorting helpers for SOA
struct UntypedSpan
{
	void* data;
	size_t count;
	size_t elemSize;
};

template<class T,class U>
void ReoderByIndices(ArenaAllocator scratchMemory, std::span<T> target, std::span<U> indices)
{
	auto cursor = MemoryArena::GetCurrentOffset(scratchMemory);
	auto tempCopy = MemoryArena::AllocCopySpan(scratchMemory, target);
	CopySpanRemappedSRC(tempCopy, target, indices);
	MemoryArena::FreeToOffset(scratchMemory,cursor);
}

void ReorderParallelOpaqueSpans(ArenaAllocator temporaryMemory, 
							 std::span<UntypedSpan> targets, 
							std::span<size_t> indices);

template<class T>
UntypedSpan ToOpaqueSpan(std::span<T> target)
{
	return {.data = (void*)target.data(), .count = target.size(), .elemSize = sizeof(target[0]) };
}

template<typename... Types>
std::span<UntypedSpan> ToUntypedSpans(ArenaAllocator allocator, Types&&... vars)
{
	constexpr auto ct = sizeof...(Types); 
	Array<UntypedSpan> opaqueSpans = MemoryArena::AllocSpan(allocator, ct);
	(opaqueSpans.push_back(ToOpaqueSpan(vars)), ...);
	return opaqueSpans.getSpan();
}

template<typename... Types>
std::span<UntypedSpan> ToUntypedSpans(Types&&... vars)
{
	constexpr auto ct = sizeof...(Types); 
	UntypedSpan opaqueSpansMemory[ct];
	Array<UntypedSpan> opaqueSpans = std::span<UntypedSpan>(opaqueSpansMemory);
	(opaqueSpans.push_back(ToOpaqueSpan(vars)), ...);
	return opaqueSpans.getSpan();
}

template<typename... Types>
void ReorderAll(ArenaAllocator temporaryMemory, std::span<size_t> indices, Types&&... vars)
{
	std::span<UntypedSpan> opaqueSpans = ToUntypedSpans(vars...);
	ReorderParallelOpaqueSpans(temporaryMemory, opaqueSpans, indices);
}


