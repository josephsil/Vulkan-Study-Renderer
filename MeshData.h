#pragma once

#define GLM_FORCE_RADIANS	
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
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
//TODO JS: multiple objects
    //TODO: Fn which loads mesh from disk and returns this struct

    //So this like, takes in a reference to the app and assigns itself to the relevant buffers/etc. Pretty gross?
    struct MeshData
    {
        
    public:
        VkBuffer vertBuffer;
        VkBuffer indexBuffer;
        VkDeviceMemory vertMemory;
        VkDeviceMemory indexMemory;
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        //TODO JS: Should meshdata own a device?
        VkPhysicalDevice physicalDevice; //Device pointers
        VkDevice device; //Device pointers

        //TODO JS: Remove verts and indices fro mthis construct when we load meshes from disk
        MeshData(HelloTriangleApplication* app, std::vector<Vertex> vertices,
            std::vector<uint32_t> indices);
       

        MeshData()
        {
        }

        //TODO JS: should probably clean itself up in a destructor or whatever 
        void cleanup();

    private:
        VkBuffer meshDataCreateVertexBuffer(HelloTriangleApplication* app);

        VkBuffer meshDataCreateIndexBuffer(HelloTriangleApplication* app);
    };
