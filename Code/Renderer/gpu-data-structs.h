#pragma once
#include "VulkanIncludes/forward-declarations-renderer.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>


struct UniformBufferObject
{
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 Normal;
    alignas(16) glm::mat4 padding1;
    alignas(16) glm::mat4 padding2;
    //Light count, vertex offset, texture index, ubo index
    alignas(16) glm::vec4 indexInfo;
    alignas(16) glm::vec4 materialprops; //roughness, metalness, padding, padding
    alignas(16) glm::vec4 Indexinfo2; //a is light index for shadows //TODO JS: remove
};

struct ShaderGlobals
{
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
    alignas(16) glm::vec4 viewPos;
    alignas(16) glm::vec4 lightcountx_modey_shadowcountz_padding_w;
    alignas(16) glm::vec4 cubemaplutidx_cubemaplutsampleridx_paddingzw;
};

struct shadowPushConstants
{
    alignas(16) glm::float32_t shadowIndex; //a is light index for shadows
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