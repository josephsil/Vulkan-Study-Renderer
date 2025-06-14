#pragma once
#include "bufferCreation.h"
#include <Renderer/RendererSharedTypes.h>


//Wrapper/convenience functions for host mapped gpu memory 
//Wraps GpuDataBuffer, which is a wrapper for gpu memory buffers (see DataBuffer.h)

template <typename T>
struct HostMappedDataBuffer //todo js: name 
{
    GpuDataBuffer buffer;
    VmaAllocation allocation;
    std::span<T> GetMappedView();
    void UpdateMappedMemory(std::span<T> source);
    uint32_t Count();
};

template <typename T>
VkDescriptorBufferInfo GetDescriptorBufferInfo(HostMappedDataBuffer<T> d)
{
    return d.buffer.getBufferInfo();
}

template <typename T>
std::span<T> HostMappedDataBuffer<T>::GetMappedView()
{
    return std::span<T>(static_cast<T*>(buffer.mapped), buffer.size / sizeof(T));
}

template <typename T>
void HostMappedDataBuffer<T>::UpdateMappedMemory(std::span<T> source)
{
    assert(source.size_bytes() == buffer.size);
    memcpy(buffer.mapped, source.data(), source.size_bytes());
}

template <typename T>
uint32_t HostMappedDataBuffer<T>::Count()
{
    return buffer.size / sizeof(T);
}

template <typename T>
HostMappedDataBuffer<T> CreateHostDataBuffer(PerThreadRenderContext* h, size_t size, VkBufferUsageFlags usage)
{
    HostMappedDataBuffer<T> hostDataBuffer{};
    hostDataBuffer.buffer.size = (uint32_t)(sizeof(T) * size);
    hostDataBuffer.buffer.mapped = BufferUtilities::CreateHostMappedBuffer(
        h->allocator, hostDataBuffer.buffer.size, usage,
        &hostDataBuffer.allocation,
        hostDataBuffer.buffer.data);

    h->threadDeletionQueue->push_backVMA(deletionType::vmaBuffer, (uint64_t)(hostDataBuffer.buffer.data),
                                           *&hostDataBuffer.allocation);
    //add to deletion queue

    return hostDataBuffer;
}