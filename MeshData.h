
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
typedef struct VkPhysicalDevice_T* VkPhysicalDevice;
typedef struct VkDevice_T* VkDevice;
typedef struct VkShaderModule_T* VkShaderModule;
typedef struct VkBuffer_T* VkBuffer;
typedef struct VkDeviceMemory_T* VkDeviceMemory;
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
