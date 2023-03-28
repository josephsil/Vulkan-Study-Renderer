#pragma once

#define GLM_FORCE_RADIANS	
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <vector>
#include "vertex.h"

#pragma region forward declarations
struct VkPipelineShaderStageCreateInfo;
struct VkPipelineShaderStageCreateInfo;
struct VkDevice_T;
struct VkShaderModule_T;
struct VkBuffer_T;
struct VkDeviceMemory_T;
struct VkPhysicalDevice_T;
typedef  VkDevice_T* VkDevice;
typedef  VkShaderModule_T* VkShaderModule;
typedef  VkBuffer_T* VkBuffer;
typedef  VkDeviceMemory_T* VkDeviceMemory;
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
        VkPhysicalDevice_T* physicalDevice; //Device pointers
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
