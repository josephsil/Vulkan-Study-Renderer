#pragma once
#include "forward-declarations-renderer.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>

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


struct UniformBufferObject
{
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 Normal;
    alignas(16) glm::mat4 padding1;
    alignas(16) glm::mat4 padding2;
};

struct ShaderGlobals
{
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
    alignas(16) glm::vec4 viewPos;
    alignas(16) glm::vec4 lightcountx_modey_shadowcountz_padding_w;
    alignas(16) glm::vec4 cubemaplutidx_cubemaplutsampleridx_paddingzw;
};

struct per_object_data
{
    //Light count, vertex offset, texture index, ubo index
    alignas(16) glm::vec4 indexInfo;

    alignas(16) glm::vec4 materialprops; //roughness, metalness, padding, padding
    alignas(16) glm::vec4 Indexinfo2; //a is light index for shadows
    alignas(16) glm::vec4 padding_2;
    alignas(16) glm::vec4 padding_3;
    //Unused
    alignas(16) glm::mat4 padding1;
    //Unused
    alignas(16) glm::mat4 padding2;
};


struct descriptorUpdateData
{
    VkDescriptorType type;
    void* ptr;
    uint32_t count = 1;
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

struct PerShadowData
{
    glm::mat4 shadowMatrix;
    float cascadeDepth;
};


struct debugLinePConstants
{
        glm::mat4 m;
        glm::vec4 pos1;
        glm::vec4 pos2;
        glm::vec4 color;
};