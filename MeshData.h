#pragma once

#define GLM_FORCE_RADIANS	
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <vector>
#include "vertex.h"


#pragma region forward declarations
#include "vulkan-forwards.h"
#pragma endregion

struct RendererHandles;

struct MeshData
{
public:
    VkBuffer vertBuffer;
    VkBuffer indexBuffer;
    VkDeviceMemory vertMemory;
    VkDeviceMemory indexMemory;
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    VkDevice device; //Device pointers
    int vertcount;
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
    VkBuffer meshDataCreateVertexBuffer(RendererHandles app);

    VkBuffer meshDataCreateIndexBuffer(RendererHandles app);
};
