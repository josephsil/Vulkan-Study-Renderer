#pragma once


#define GLM_FORCE_RADIANS	
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <vector>
#include "vertex.h"
#include "RendererContext.h"

#pragma region forward declarations
#include <span>

#include "gpu-data-structs.h"
#include "VulkanIncludes/forward-declarations-renderer.h"
#pragma endregion

//TODO JS, use this temporarily when building the mesh.... not sure what to do with it
struct temporaryloadingMesh
{
    std::span<Vertex> vertices; 
    std::span<uint32_t> indices;
    bool tangentsLoaded = false;
    MemoryArena::memoryArena* temporaryArena;
    ptrdiff_t expectedArenaHead;
};

struct MeshData
{
public:
   
    std::span<Vertex> vertices; 
    std::span<uint32_t> indices; 
    std::span<glm::vec3> boundsCorners;
    int id;




    //TODO JS: should probably clean itself up in a destructor or whatever 
    void cleanup();

};



positionRadius boundingSphereFromMeshBounds(std::span<glm::vec3> boundsCorners);
MeshData MeshDataFromSpans(std::span<Vertex> vertices,
         std::span<uint32_t> indices);
MeshData MeshDataFromObjFile(RendererContext rendererHandles, const char* path);
MeshData FinalizeMeshDataFromTempMesh(MemoryArena::memoryArena* outputArena, temporaryloadingMesh tempMesh);