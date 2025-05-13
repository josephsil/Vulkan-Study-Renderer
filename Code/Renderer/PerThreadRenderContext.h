#pragma once

#include "RendererDeletionQueue.h"
#include "VulkanIncludes/forward-declarations-renderer.h"
#include <mutex>

#include "CommandPoolManager.h"

namespace vkb
{
    struct Device;
}

struct deleteableResource;

namespace MemoryArena
{
    struct memoryArena;
}

//Renderer stuff various subsystems will need to access
struct PerThreadRenderContext
{
    VkPhysicalDevice physicalDevice;
    VkDevice device;
    CommandPoolManager* textureCreationcommandPoolmanager;
    VmaAllocator allocator;
    MemoryArena::memoryArena* arena;
    MemoryArena::memoryArena* tempArena;
    RendererDeletionQueue* threadDeletionQueue;
    VkFence ImportFence;
    vkb::Device* vkbd;
};

//Like renderercontext, but doesn't need cpu memory arenas or the physical device
struct BufferCreationContext
{
    VkDevice device;
    VmaAllocator allocator;
    RendererDeletionQueue* rendererdeletionqueue;
    CommandPoolManager* commandPoolmanager;
};

inline BufferCreationContext objectCreationContextFromRendererContext(PerThreadRenderContext r)
{
    return {
        .device = r.device, .allocator = r.allocator, .rendererdeletionqueue = r.threadDeletionQueue,
        .commandPoolmanager = r.textureCreationcommandPoolmanager
    };
}
