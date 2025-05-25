static const uint SHADOW_MAP_SIZE = 1024;
static const float POINT_LIGHT_FAR_PLANE = 10.0;
static const float POINT_LIGHT_NEAR_PLANE = 0.01f;
struct Transform
{
    float4x4 Model;
    float4x4 NormalMat;
    //objectProperties
    //Formerly push constants
};  
struct Bounds
{
    float2 min;
    float2 max;
};

struct BoundingSphere
{
    float4 center;
    float radius;
};

struct objectProperties
{
    float4 indexInfo;
    float4 textureIndexInfo;
    float roughness;
    float metallic;
    float _f1;
    float _f2;
    float4 color;
 
};

struct ObjectData
{
    //objectProperties
    objectProperties props;

    //Culling info
    //This data is really per model, not per object, but I'm lazy
    BoundingSphere boundsSphere;

    //bounding box
    Bounds bounds;
    float4 boundsmin;
    float4 boundsmax;

};

struct ShaderGlobals
{
    float4x4 viewMatrix;
    float4x4 projMatrix;
    float4 eyePos;
    float4 lightcount_mode_shadowct_padding;
    float4 lutIDX_lutSamplerIDX_padding_padding;
};

struct perShadowData
{
    float4x4 viewMatrix;
    float4x4 projMatrix;
    float depth;
};//

struct VertexData
{
    float4 uv0;
    float4 normal;
    float4 Tangent;
    // uint color;
};

struct LightData
{
    float4 position_range;
    float4 color_intensity;
    float4 lighttype_lightDir;
    uint shadowOffset;
    uint shadowCount; 
};



struct shadowPushConstant
{
    float unused;
    float4x4 mat;
};


struct CullPushConstants
{
    float4x4 viewMatrix;
    float4x4 projMatrix;
    uint offset;
    uint frustumOffset;
    uint objectCount;
    //TODO JS: frustum should just go in here
};

struct DebugLinePushConstants
{
    float4x4 m; // always identity
    float4 pos1;
    float4 pos2;
    float4 color;
};