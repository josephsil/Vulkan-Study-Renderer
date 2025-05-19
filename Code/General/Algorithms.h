#pragma once
#include <cassert>
#include <span>

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
inline void FillIndicesAsdencing(std::span<T> numbers)
{
    static_assert( std::is_unsigned_v<T>, "U must be an unsigned type" );
    for (T i = 0; i < numbers.size(); i++)
    {
        numbers[i] = i;
    }
    
}