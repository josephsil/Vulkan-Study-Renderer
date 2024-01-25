#pragma once
#include <cassert>
#include <span>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <glm/common.hpp>

template < typename T >
struct Array
{
    T* data = nullptr;
    size_t ct;
    size_t capacity;


    Array(){};
    Array(T* d, size_t _size)
    {
        assert(!std::is_void_v<T>);
        data = d;
        capacity = _size;
        ct = 0;
    }

    Array(T* d, size_t _ct, size_t _size)
    {
     assert(!std::is_void_v<T>);
        data = d;
        capacity = _size;
        ct = _ct;
    }

    Array(std::span<T> span)
    {
     assert(!std::is_void_v<T>);
        data = span.data();
        capacity = span.size();
        ct = 0;
    }
    
    T& operator[](const size_t i)
    {
        assert(i < ct);
        return data[i];
    }

    void push_back(T entry)
    {
        assert (ct < capacity);
        data[ct++] = entry;
    }

    std::span<T> getSpan(size_t size = INT_MAX)
    {
        return std::span<T>(data, glm::min(ct, size));
    }

    inline size_t size() const { return ct; }
    inline size_t size_bytes() const { return sizeof(T) * ct; }
    inline size_t capacity_bytes() const { return sizeof(T) * capacity;}
    
};
