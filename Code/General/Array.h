#pragma once
#include <cassert>
#include <span>
#ifndef NOMINMAX
#define NOMINMAX
#endif

template <typename T>
struct Array
{
    T* data = nullptr;
    size_t ct;
    size_t capacity;


    Array()
    {
    };

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

    T& back()
    {
        return data[ct - 1];
    }

    T& push_back()
    {
        assert(ct < capacity);
        ct++;
        return data[ct -1];
    }

    void push_back(T entry)
    {
        if (!std::is_copy_constructible<T>::value)
        {
            assert(!"push_back with non-copyable type T");
            return;
        }
        assert(ct < capacity);
        data[ct++] = entry;
    }
    
    template <typename... Args>
    void emplace_back(Args&&... args)
    {
        assert(ct < capacity);
        new (&data[ct++]) T(std::forward<Args>(args)...);
    }

    std::span<T> push_back_span(std::span<T> s)
    {
        assert(ct +  s.size() <capacity);
        size_t start = ct;
        for (auto entry :  s)
        {
            this->push_back(entry);
        }
        return std::span<T>((T*)(data+start),  s.size());
    }

    std::span<T> push_back_span(std::initializer_list<T> il)
    {
        assert(ct + il.size() <capacity);
        int start = ct;
        for (auto entry : il)
        {
            this->push_back(entry);
        }
        std::span<T>(start, il.size());
    }

    std::span<T> pushUninitializedSubspan(size_t spanSize)
    {
        assert(ct + spanSize < capacity);
        size_t start = ct;
        ct += spanSize;
        return this->getSpan().subspan(start, spanSize);
    }

    size_t _min(size_t a, size_t b) {
        return (a < b) ? a : b;
    }
    std::span<T> getSpan(size_t size = INT_MAX)
    {
        return std::span<T>(data, _min(ct, size));
    }
    
    std::span<T> getSubSpanToCapacity(size_t offset = 0)
    {
        return std::span<T>(&data[offset], capacity - offset);
    }

    size_t size() const { return ct; }
    size_t size_bytes() const { return sizeof(T) * ct; }
    size_t capacity_bytes() const { return sizeof(T) * capacity; }
};
