//GENERATED FILE
//EDIT structs.hlsl TO UPDATE
#pragma once
#include <Renderer/VulkanIncludes/forward-declarations-renderer.h>
#include <General/GLM_impl.h>
static constexpr uint32_t SHADOW_MAP_SIZE = 1024;
static constexpr uint32_t DEPTH_PYRAMID_SIZE = SHADOW_MAP_SIZE;
static constexpr float POINT_LIGHT_FAR_PLANE = 10.0f;
static constexpr float POINT_LIGHT_NEAR_PLANE = 0.01f;
static constexpr float CAMERA_NEAR_PLANE = 0.01f;
static constexpr uint32_t CASCADE_CT = 6;
static constexpr uint32_t CULL_WORKGROUP_X = 64;
static constexpr uint32_t COPY_WORKGROUP_X = CULL_WORKGROUP_X;
static constexpr uint32_t MIP_WORKGROUP_X = 16;
static constexpr uint32_t MIP_WORKGROUP_Y = MIP_WORKGROUP_X;
struct GPU_Transform
{
    glm::mat4 Model;
    glm::mat4 NormalMat;
    //GPU_objectProperties
    //Formerly push constants
};  
struct GPU_Bounds
{
    glm::vec4  min;
    glm::vec4  max;
};

struct GPU_BoundingSphere
{
    glm::vec4  center;
    float radius;
};

struct GPU_objectProperties
{
    glm::vec4  indexInfo;
    glm::vec4  textureIndexInfo;
 
    float roughness;
    float metallic;
    float _f1;
    float _f2;
    glm::vec4  color;
 
};

struct GPU_ObjectData
{
    //GPU_objectProperties
    GPU_objectProperties props;
    float objectScale;
    //Culling info
    //This data is really per model, not per object, should move
    GPU_BoundingSphere boundsSphere;

    //bounding box
    GPU_Bounds bounds;

};

struct GPU_ShaderGlobals
{
    glm::mat4 viewMatrix;
    glm::mat4 projMatrix;
    glm::vec4  eyePos;
    glm::vec4  lightcount_mode_shadowct_padding;
    glm::vec4  lutIDX_lutSamplerIDX_padding_padding;
};

struct GPU_perShadowData
{
    glm::mat4 viewMatrix;
    glm::mat4 projMatrix;
    float farPlane;
    float nearPlane;
    float depth;
};//

struct GPU_VertexData
{
    glm::vec4  uv0;
    glm::vec4  normal;
    glm::vec4  Tangent;
    // uint32_t color;
};

struct GPU_LightData
{
    glm::vec4  position_range;
    glm::vec4  color_intensity;
    glm::vec4  lighttype_lightDir;
    uint32_t shadowOffset;
    uint32_t shadowCount; 
};



struct GPU_shadowPushConstant
{
    float unused;
    glm::mat4 mat;
};


struct GPU_CullPushConstants
{
    glm::mat4 viewMatrix;
    glm::mat4 projMatrix;
    uint32_t drawOffset;
    uint32_t passOffset;
    uint32_t frustumOffset;
    uint32_t objectCount;
    uint32_t LATE_CULL;
    uint32_t disable;
    float farPlane;
    float nearPlane;
    //TODO JS: frustum should just go in here
};

struct GPU_DebugLinePushConstants
{
    glm::mat4 m; // always identity
    glm::vec4  pos1;
    glm::vec4  pos2;
    glm::vec4  color;
};

struct GPU_drawCommandData
{
    uint32_t objectIndex;
    uint32_t indexCount;
    uint32_t instanceCount;
    uint32_t firstIndex;
    int vertexOffset;
    uint32_t firstInstance;
};
