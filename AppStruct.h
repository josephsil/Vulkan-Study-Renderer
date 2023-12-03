#pragma once

#include "forward-declarations-renderer.h"
struct CommandPoolManager;

namespace  MemoryArena
{
    struct memoryArena;
}
    //TODO JS: pass an arena in

//This is stuff that I (currently) have to pass around a lot
struct RendererHandles
{
    VkPhysicalDevice physicalDevice;
    VkDevice device; //Logical device
    CommandPoolManager* commandPoolmanager;
    VmaAllocator allocator;
    MemoryArena::memoryArena* arena;
    MemoryArena::memoryArena* perframeArena;
    bool canWriteKTX;
};