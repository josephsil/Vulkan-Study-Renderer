#pragma once

#define GLM_FORCE_RADIANS	
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <vector>
#include "vertex.h"
#include "RendererHandles.h"

#pragma region forward declarations
#include <span>

#include "VulkanIncludes/forward-declarations-renderer.h"
#pragma endregion


struct MeshData
{
public:
    VkBuffer vertBuffer;
    VkBuffer indexBuffer;
    RendererHandles rendererHandles; //TODO to pointer?
    VmaAllocation vertMemory;
    VmaAllocation indexMemory;
    std::span<Vertex> vertices; // Span into memoryArena
    std::span<uint32_t> indices; //Span into memoryArena
    VkDevice device; //Device pointers
    size_t vertcount;
    int id;


    MeshData(RendererHandles rendererHandles, std::vector<Vertex> vertices,
             std::vector<uint32_t> indices);

    MeshData(RendererHandles rendererHandles, const char* path);

    MeshData()
    {
    }

    //TODO JS: should probably clean itself up in a destructor or whatever 
    void cleanup();

private:
    VkBuffer meshDataCreateVertexBuffer();

    VkBuffer meshDataCreateIndexBuffer();
};
