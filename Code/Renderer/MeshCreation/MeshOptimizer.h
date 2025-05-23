#pragma once
#include <span>

#include "General/MemoryArena.h"

struct MeshData2;
struct MeshData;
namespace MeshOptimizer
{
    MeshData2 RunMeshOptimizer(ArenaAllocator arena, MeshData input);
}
