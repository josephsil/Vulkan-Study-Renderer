#pragma once


#define GLM_FORCE_RADIANS	
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_LEFT_HANDED
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <Renderer/Vertex.h>

#pragma region forward declarations
#include <span>

#include <Renderer/gpu-data-structs.h>
#pragma endregion

namespace MemoryArena
{
    struct Allocator;
}

struct PerThreadRenderContext;

//TODO JS, use this temporarily when building the mesh.... not sure what to do with it
struct temporaryloadingMesh
{
    std::span<Vertex> vertices;
    std::span<uint32_t> indices;
    bool tangentsLoaded = false;
};

//This is what we get out of the end of the mesh asset importers
//Underlying vertices and indices should already have been allocated with RequestMeshMemory
//Filling out and submitting this to AssetManager::AddMesh builds out submesh data and finalizes it.
struct ImportedMeshData
{
    uint32_t vertexct;
    std::span<uint32_t> indexCounts;
    std::span<uint32_t> meshletVertexOffsets; //Redundant to PersubmeshData offset -- used to construct it, need to get rid of this
    std::span<GPU_BoundingSphere> meshletBounds; //Redundant to PersubmeshData bounds  -- used to construct it, need to get rid of this
    uint32_t meshletCount;
};

//This is what we load from external mesh formats (gltf, obj)
//This is fed to MeshOptimizer to produce ImportedMeshData
struct InterchangeMesh
{
   std::span<Vertex> vertices;
   std::span<uint32_t>indices;
    std::span<uint32_t> indexCounts;
   std::span<glm::vec3> boundsCorners;
};

//Mesh import functionality
namespace MeshDataCreation
{
    GPU_BoundingSphere CreateBoundingSphereFromMeshBounds(std::span<glm::vec3> boundsCorners);
    InterchangeMesh MeshDataFromSpans(std::span<Vertex> vertices,
                               std::span<uint32_t> indices);
    InterchangeMesh MeshDataFromObjFile(PerThreadRenderContext rendererHandles, const char* path);
    InterchangeMesh FinalizeMeshDataFromTempMesh(MemoryArena::Allocator* outputArena, MemoryArena::Allocator* tempArena,
                                          temporaryloadingMesh tempMesh);
}
