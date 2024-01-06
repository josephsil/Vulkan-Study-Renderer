#pragma once

#include "forward-declarations-renderer.h"
struct CommandPoolManager;


const static int MAX_SHADOWCASTERS = 8;
const static int CASCADE_CT = 4;
#define MAX_SHADOWMAPS (MAX_SHADOWCASTERS * 6)
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