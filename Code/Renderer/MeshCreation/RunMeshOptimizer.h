#pragma once
#include <span>

#include "General/MemoryArena.h"

struct ImportedMeshData;
struct InterchangeMesh;
class AssetManager;

//Runs MeshOptimizer to produce meshlets on from imported meshes
namespace MeshOptimizer
{
    ImportedMeshData RunMeshOptimizer(ArenaAllocator arena, AssetManager& rendererData, InterchangeMesh input);
}
