#pragma once

#include "RendererDeletionQueue.h"
#include "General/Array.h"
#include "VulkanIncludes/forward-declarations-renderer.h"

struct CommandPoolManager;
struct deleteableResource;

namespace MemoryArena
{
    struct memoryArena;
}

//TODO JS: pass an arena in

//Renderer stuff various subsystems will need to access
struct RendererContext
{
    VkPhysicalDevice physicalDevice;
    VkDevice device;
    CommandPoolManager* textureCreationcommandPoolmanager;
    VmaAllocator allocator;
    MemoryArena::memoryArena* arena;
    MemoryArena::memoryArena* tempArena;
    RendererDeletionQueue* rendererdeletionqueue;
};

//Like renderercontext, but doesn't need cpu memory arenas or the physical device
struct BufferCreationContext
{
    VkDevice device;
    VmaAllocator allocator;
    RendererDeletionQueue* rendererdeletionqueue;
    CommandPoolManager* commandPoolmanager;
};

inline BufferCreationContext objectCreationContextFromRendererContext(RendererContext r)
{
    return {
        .device = r.device, .allocator = r.allocator, .rendererdeletionqueue = r.rendererdeletionqueue,
        .commandPoolmanager = r.textureCreationcommandPoolmanager
    };
}
