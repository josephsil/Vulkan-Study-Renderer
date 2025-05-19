#pragma once
#include <span>

#include "General/MemoryArena.h"

struct MeshData;

namespace MeshOptimizer
{
    std::span<MeshData>  RunMeshOptimizer(ArenaAllocator arena, MeshData input);
}
