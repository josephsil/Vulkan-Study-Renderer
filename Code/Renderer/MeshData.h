#pragma once


#define GLM_FORCE_RADIANS	
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <vector>
#include "vertex.h"
#include "RendererHandles.h"

#pragma region forward declarations
#include <span>

#include "gpu-data-structs.h"
#include "VulkanIncludes/forward-declarations-renderer.h"
#pragma endregion


struct MeshData
{
public:
   
    std::span<Vertex> vertices; // Span into memoryArena
    std::span<uint32_t> indices; //Span into memoryArena
    std::span<glm::vec3> boundsCorners;
    int id;




    //TODO JS: should probably clean itself up in a destructor or whatever 
    void cleanup();

};



positionRadius boundingSphereFromMeshBounds(std::span<glm::vec3> boundsCorners);

MeshData MeshDataFromSpans(std::vector<Vertex> vertices,
         std::vector<uint32_t> indices);

MeshData MeshDataFromFile(RendererHandles rendererHandles, const char* path);