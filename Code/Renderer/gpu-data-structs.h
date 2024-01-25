#pragma once
#include "VulkanIncludes/forward-declarations-renderer.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>

struct __VkDrawIndirectCommand {
    uint32_t    vertexCount;
    uint32_t    instanceCount;
    uint32_t    firstVertex;
    uint32_t    firstInstance;
};

struct objectProperties
{
    alignas(16) glm::vec4 indexInfo; //Light count, vertex offset, texture index, ubo index
    alignas(16) glm::vec4 materialprops; //roughness, metalness, padding, padding
};

struct positionRadius
{
    alignas(16) glm::vec4 objectSpacePos;
    alignas(16) glm::float32_t objectSpaceRadius;
};

struct UniformBufferObject
{
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 Normal;
    //Per object properties -- indices and material props
    objectProperties props;
    
    //Culling info
    //This data is really per model, not per object, but I'm lazy
    positionRadius cullingInfo;
};


struct ShaderGlobals
{
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
    alignas(16) glm::vec4 viewPos;
    alignas(16) glm::vec4 lightcountx_modey_shadowcountz_padding_w;
    alignas(16) glm::vec4 cubemaplutidx_cubemaplutsampleridx_paddingzw;
};

struct drawCommandData
{
    uint32_t objectIndex;
    // VkDrawIndirectCommand
    // alignas(16) glm::vec4 debugdata;
    __VkDrawIndirectCommand command;
};


struct shadowPushConstants
{
    alignas(16) glm::float32_t shadowIndex; //a is light index for shadows
};



struct PerShadowData
{
    glm::mat4 view;
    glm::mat4 proj;
    float cascadeDepth;
};

struct gpuPerShadowData
{
    glm::mat4 view;
    glm::mat4 proj;
    float cascadeDepth;
};


struct debugLinePConstants
{
        glm::mat4 m;
        glm::vec4 pos1;
        glm::vec4 pos2;
        glm::vec4 color;
};

//Compute
struct cullPConstants
{
    alignas(16) glm::mat4 view;
    uint32_t firstDraw;
    uint32_t frustumIndex;
    uint32_t objectCount;
};



struct gpuvertex
{
    alignas(16) glm::vec4 pos;
    alignas(16) glm::vec4 texCoord;
    alignas(16) glm::vec4 normal;
    alignas(16) glm::vec4 tangent;
};


struct gpulight
{
    alignas(16) glm::vec4 pos_xyz_range_a;
    alignas(16) glm::vec4 color_xyz_intensity_a;
    alignas(16) glm::vec4 pointOrSpot_x_dir_yza;
    // alignas(16) glm::mat4 matrixViewProjection;
    alignas(16) glm::vec4 matrixIndex_matrixCount_padding_padding; // currently only used by point
};