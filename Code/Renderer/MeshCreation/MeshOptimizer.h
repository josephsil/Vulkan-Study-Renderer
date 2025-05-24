#pragma once
#include <span>

#include "General/MemoryArena.h"

struct ImportMeshData;
struct preMeshletMesh;
class AssetManager;
namespace MeshOptimizer
{
    ImportMeshData RunMeshOptimizer(ArenaAllocator arena, AssetManager& rendererData, preMeshletMesh input);
}
