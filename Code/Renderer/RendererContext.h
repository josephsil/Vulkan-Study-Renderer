#pragma once

#include "RendererDeletionQueue.h"
#include "General/Array.h"
#include "VulkanIncludes/forward-declarations-renderer.h"
struct CommandPoolManager;
struct deleteableResource;
namespace  MemoryArena
{
    struct memoryArena;
}
    //TODO JS: pass an arena in

//This is stuff that I (currently) have to pass around a lot
struct RendererContext
{
    VkPhysicalDevice physicalDevice;
    VkDevice device; //Logical device
    CommandPoolManager* commandPoolmanager;
    VmaAllocator allocator;
    MemoryArena::memoryArena* arena;
    MemoryArena::memoryArena* perframeArena;
    RendererDeletionQueue* rendererdeletionqueue;
};

struct rendererContext_NEW //gonna use this for textures -- may rename, may phase out other one
{
    VkDevice device;
    VmaAllocator allocator;
    MemoryArena::memoryArena* memoryArena;
    RendererDeletionQueue* rendererdeletionqueue;
};
