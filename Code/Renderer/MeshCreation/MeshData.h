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

struct RendererContext;

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
    std::span<uint32_t> indices;
    std::span<glm::vec3> boundsCorners;
    int id; //TODO JS: needed?
};

namespace MeshDataCreation
{
    positionRadius boundingSphereFromMeshBounds(std::span<glm::vec3> boundsCorners);
    MeshData MeshDataFromSpans(std::span<Vertex> vertices,
                               std::span<uint32_t> indices);
    MeshData MeshDataFromObjFile(RendererContext rendererHandles, const char* path);
    MeshData FinalizeMeshDataFromTempMesh(MemoryArena::memoryArena* outputArena, MemoryArena::memoryArena* tempArena,
                                          temporaryloadingMesh tempMesh);
}
