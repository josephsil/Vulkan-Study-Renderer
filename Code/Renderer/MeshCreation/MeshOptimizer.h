#pragma once
#include <span>

#include "General/MemoryArena.h"

struct MeshData;
struct preMeshletMesh;
namespace MeshOptimizer
{
    MeshData RunMeshOptimizer(ArenaAllocator arena, preMeshletMesh input);
}
