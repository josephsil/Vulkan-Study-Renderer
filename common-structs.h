#pragma once
#include "forward-declarations-renderer.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
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
    alignas(16) glm::vec4 lightcountx_modey_paddingzw;
    alignas(16) glm::vec4 cubemaplutidx_cubemaplutsampleridx_paddingzw;
};

struct per_object_data
{
    //Light count, vertex offset, texture index, ubo index
    alignas(16) glm::vec4 indexInfo;

    alignas(16) glm::vec4 materialprops; //roughness, metalness, padding, padding
    alignas(16) glm::vec4 padding_1;
    alignas(16) glm::vec4 padding_2;
    alignas(16) glm::vec4 padding_3;
    //Unused
    alignas(16) glm::mat4 padding1;
    //Unused
    alignas(16) glm::mat4 padding2;
};
   

