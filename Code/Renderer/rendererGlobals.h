#pragma once
#include <glm/glm.hpp>
#include <vector>
#include "VulkanIncludes/forward-declarations-renderer.h"
const static int MAX_SHADOWCASTERS = 8;
const static int CASCADE_CT = 4;
#define MAX_SHADOWMAPS (MAX_SHADOWCASTERS * 6)

const static int MAX_FRAMES_IN_FLIGHT = 3;

enum lightType
{
    LIGHT_DIR = 0,
    LIGHT_POINT = 1,
    LIGHT_SPOT = 2,
};
struct RenderTextureFormat
{
    VkFormat ColorFormat;
    VkFormat DepthFormat;
};

struct bufferAndPool
{
    VkCommandBuffer buffer;
    VkCommandPool pool;
    VkQueue queue;
    std::vector<VkSemaphore> waitSemaphores = {};
    std::vector<VkSemaphore> signalSemaphores = {};
};



struct descriptorUpdateData
{
    VkDescriptorType type;
    void* ptr;
    uint32_t count = 1;
    // layoutInfo* layout = NULL; //TODO JS: set this at creation time
};

struct dataBuffer
{
    VkBuffer data;
    uint32_t size;
    void* mapped;

    VkDescriptorBufferInfo getBufferInfo();
    void updateMappedMemory(void* data, size_t size);
};


struct inputData
{
    glm::vec3 keyboardMove;
    glm::vec2 mouseRot;
};

struct Transform
{

    glm::mat4 translation;
    glm::mat4 rot; 
};


struct Material
{
public:

    uint32_t pipelineidx;
    int backingTextureidx;
    bool metallic;
    float roughness;
};
