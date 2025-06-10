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

template<class T,class U>
inline void CopySpanRemappedDST(std::span<T> src, std::span<T> dst, std::span<U> dstIndices)
{
 static_assert( std::is_unsigned_v<U>, "U must be an unsigned type" );
    for (size_t i = 0; i < dstIndices.size(); i++)
    {
        dst[dstIndices[i]] = src[i];
    }
    
}
template<class T,class U>
inline void CopySpanRemappedBoth(std::span<T> src, std::span<T> dst, std::span<U> srcIndices,  std::span<U> dstIndices)
{
 static_assert( std::is_unsigned_v<U>, "U must be an unsigned type" );
    assert(srcIndices.size() == dstIndices.size());
    for (size_t i = 0; i < srcIndices.size(); i++)
    {
        dst[dstIndices[i]] = src[srcIndices[i]];
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
struct OpaqueSpan
{
	void* data;
	size_t count;
	size_t size;
};

template<class T,class U>
void ReoderByIndices(ArenaAllocator scratchMemory, std::span<T> target, std::span<U> indices)
{
	MemoryArena::setCursor(scratchMemory);
	auto tempCopy = MemoryArena::copySpan(scratchMemory, target);
	CopySpanRemappedSRC(tempCopy, target, indices);
	MemoryArena::freeToCursor(scratchMemory);


}

void ReorderParallelOpaqueSpans(ArenaAllocator scratchMemory, 
							 std::span<OpaqueSpan> targets, 
							std::span<size_t> indices);

template<class T>
OpaqueSpan ToOpaqueSpan(std::span<T> target)
{
	return {
		.data = (void*)target.data(), .count = target.size(), .size = sizeof(target[0]) };
}

template<typename... Types>
std::span<OpaqueSpan> ConvertToOpaqueSpans(ArenaAllocator scratchMemory, Types&&... vars)
{
	const auto ct = sizeof...(Types); //Hacky -- counting up the number of args by testing a predicate which is always true
	Array<OpaqueSpan> opaqueSpans = MemoryArena::AllocSpan<OpaqueSpan>(scratchMemory, ct);
	(opaqueSpans.push_back(ToOpaqueSpan(vars)), ...);
	return opaqueSpans.getSpan();
}

template<typename... Types>
void ReorderSOA(ArenaAllocator scratchMemory, std::span<size_t> indices, Types&&... vars)
{
	std::span<OpaqueSpan> opaqueSpans = ConvertToOpaqueSpans(scratchMemory, vars...);
	ReorderParallelOpaqueSpans(scratchMemory, opaqueSpans, indices);
}


