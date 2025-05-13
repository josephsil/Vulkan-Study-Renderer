#pragma once
#include "bufferCreation.h"
#include <Renderer/RendererSharedTypes.h>

template <typename T>
struct HostDataBufferObject //todo js: name 
{
    dataBuffer buffer;
    VmaAllocation allocation;
    std::span<T> getMappedSpan();
    void updateMappedMemory(std::span<T> source);
    uint32_t count();
};


template <typename T>
VmaAllocation dataBuffer_getAllocation(HostDataBufferObject<T> d)
{
    return d.allocation;
}

template <typename T>
VkBuffer dataBuffer_getVKBuffer(HostDataBufferObject<T> d)
{
    return d.buffer.data;
}


template <typename T>
VkDescriptorBufferInfo GetDescriptorBufferInfo(HostDataBufferObject<T> d)
{
    return d.buffer.getBufferInfo();
}

template <typename T>
std::span<T> HostDataBufferObject<T>::getMappedSpan()
{
    return std::span<T>(static_cast<T*>(buffer.mapped), buffer.size / sizeof(T));
}

template <typename T>
void HostDataBufferObject<T>::updateMappedMemory(std::span<T> source)
{
    assert(source.size_bytes() == buffer.size);
    memcpy(buffer.mapped, source.data(), source.size_bytes());
}

template <typename T>
uint32_t HostDataBufferObject<T>::count()
{
    return buffer.size / sizeof(T);
}

template <typename T>
HostDataBufferObject<T> createDataBuffer(PerThreadRenderContext* h, size_t size, VkBufferUsageFlags usage)
{
    HostDataBufferObject<T> hostDataBuffer{};
    hostDataBuffer.buffer.size = (uint32_t)(sizeof(T) * size);
    hostDataBuffer.buffer.mapped = BufferUtilities::createHostMappedBuffer(
        h->allocator, hostDataBuffer.buffer.size, usage,
        &hostDataBuffer.allocation,
        hostDataBuffer.buffer.data);

    h->threadDeletionQueue->push_backVMA(deletionType::vmaBuffer, (uint64_t)(hostDataBuffer.buffer.data),
                                           *&hostDataBuffer.allocation);
    //add to deletion queue

    return hostDataBuffer;
}