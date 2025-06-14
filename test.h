#PRAGMA ONCE
#include "VulkanIncludes/forward-declarations-renderer.h
#include <General/GLM_IMPL.h>
struct GPU_transformdata
{
    alignas(16) glm::vec4 x4 Model;
    alignas(16) glm::vec4 x4 NormalMat;
    //objectProperties
    //Formerly push constants
};

struct GPU_objectData
{
    //objectProperties
    alignas(16) glm::vec4  indexInfo;
    alignas(16) glm::vec4  textureIndexInfo;
    float roughness;
    float metallic;
    float _f1;
    float _f2;
    alignas(16) glm::vec4  color;

    //Culling info
    //This data is really per model, not per object, but I'm lazy
    alignas(16) glm::vec4  objectSpaceboundsCenter;
    float objectSpaceboundsRadius;

    //bounding box
    alignas(16) glm::vec4  boundsmin;
    alignas(16) glm::vec4  boundsmax;

};

struct GPU_ShaderGlobals
{
    alignas(16) glm::vec4 x4 view;
    alignas(16) glm::vec4 x4 projection;
    alignas(16) glm::vec4  viewPos;
    alignas(16) glm::vec4  lightcount_mode_shadowct_padding;
    alignas(16) glm::vec4  lutIDX_lutSamplerIDX_padding_padding;
};

struct GPU_perShadowData
{
    alignas(16) glm::vec4 x4 view;
    alignas(16) glm::vec4 x4 proj;
    float depth;
};

struct GPU_MyVertexStructure
{
    alignas(16) glm::vec4  uv0;
    alignas(16) glm::vec4  normal;
    alignas(16) glm::vec4  Tangent;
    // uint32_t color;
};

struct GPU_MyLightStructure
{
    alignas(16) glm::vec4  position_range;
    alignas(16) glm::vec4  color_intensity;
    alignas(16) glm::vec4  lighttype_lightDir;
    alignas(16) glm::vec4  matrixIDX_matrixCt_padding; // currently only used by point
    // alignas(16) glm::vec4 x4 _DELETED; //TODO JS: delete for real
    // uint32_t color;
};

