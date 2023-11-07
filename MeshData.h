#pragma once

#define GLM_FORCE_RADIANS	
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <string>
#include <vector>
#include "vertex.h"

#pragma region forward declarations
struct VkPipelineShaderStageCreateInfo;
struct VkPipelineShaderStageCreateInfo;
using VkPhysicalDevice = struct VkPhysicalDevice_T*;
using VkDevice = struct VkDevice_T*;
using VkShaderModule = struct VkShaderModule_T*;
using VkBuffer = struct VkBuffer_T*;
using VkDeviceMemory = struct VkDeviceMemory_T*;
#pragma endregion

class HelloTriangleApplication;

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


    MeshData(HelloTriangleApplication* app, std::vector<Vertex> vertices,
             std::vector<uint32_t> indices);

    MeshData(HelloTriangleApplication* app, std::string path);

    MeshData()
    {
    }

    //TODO JS: should probably clean itself up in a destructor or whatever 
    void cleanup();

private:
    VkBuffer meshDataCreateVertexBuffer(HelloTriangleApplication* app);

    VkBuffer meshDataCreateIndexBuffer(HelloTriangleApplication* app);
};
