#pragma once


#define GLM_FORCE_RADIANS	
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include "../vertex.h"

#pragma region forward declarations
#include <span>

#include "../gpu-data-structs.h"
#pragma endregion

namespace MemoryArena
{
    struct memoryArena;
}

struct PerThreadRenderContext;

//TODO JS, use this temporarily when building the mesh.... not sure what to do with it
struct temporaryloadingMesh
{
    std::span<Vertex> vertices;
    std::span<uint32_t> indices;
    bool tangentsLoaded = false;
};

struct MeshData
{
    std::span<Vertex> vertices;
    std::span<uint8_t> meshletsIndices;
    std::span<uint32_t> indexCounts;
    std::span<uint32_t> meshletVertexOffsets; //Redundant to PersubmeshData offset -- used to construct it, need to get rid of this
    std::span<std::span<glm::vec3>> meshletBounds; //Redundant to PersubmeshData bounds  -- used to construct it, need to get rid of this
    uint32_t meshletCount;
};

struct preMeshletMesh
{
    std::span<Vertex> vertices;
   std::span<uint32_t>indices;
   std::span<glm::vec3> boundsCorners;
};

namespace MeshDataCreation
{
    positionRadius boundingSphereFromMeshBounds(std::span<glm::vec3> boundsCorners);
    preMeshletMesh MeshDataFromSpans(std::span<Vertex> vertices,
                               std::span<uint32_t> indices);
    preMeshletMesh MeshDataFromObjFile(PerThreadRenderContext rendererHandles, const char* path);
    preMeshletMesh FinalizeMeshDataFromTempMesh(MemoryArena::memoryArena* outputArena, MemoryArena::memoryArena* tempArena,
                                          temporaryloadingMesh tempMesh);
}
