#pragma once
#include <cassert>
#include <span>

template < typename T >
struct Array
{
    T* data = nullptr;
    size_t ct;
    size_t capacity;


    Array(T* d, size_t _size)
    {
        data = d;
        capacity = _size;
        ct = 0;
    }

    Array(T* d, size_t _ct, size_t _size)
    {
        data = d;
        capacity = _size;
        ct = _ct;
    }

    explicit Array(std::span<T> span)
    {
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
        assert (ct + 1 < capacity);
        data[ct++] = entry;
    }

    inline size_t size() const { return ct; }
    inline size_t size_bytes() const { return sizeof(T) * ct; }
    inline size_t capacity_bytes() const { return sizeof(T) * size; }
    
};
