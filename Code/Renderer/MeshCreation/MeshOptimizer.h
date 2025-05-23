#pragma once
#include <span>

#include "General/MemoryArena.h"

struct ImportMeshData;
struct preMeshletMesh;
namespace MeshOptimizer
{
    ImportMeshData RunMeshOptimizer(ArenaAllocator arena, preMeshletMesh input);
}
