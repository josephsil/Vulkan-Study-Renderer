struct objectData
{
    float4x4 Model;
    float4x4 NormalMat;

    //objectProperties
    //Formerly push constants
    float4 indexInfo;
    float4 textureIndexInfo;
    float roughness;
    float metallic;
    float _f1;
    float _f2;
    float4 color;

    //Culling info
    //This data is really per model, not per object, but I'm lazy
    float4 objectSpaceboundsCenter;
    float objectSpaceboundsRadius;
};

struct ShaderGlobals
{
    float4x4 view;
    float4x4 projection;
    float4 viewPos;
    float4 lightcount_mode_shadowct_padding;
    float4 lutIDX_lutSamplerIDX_padding_padding;
};

struct perShadowData
{
    float4x4 view;
    float4x4 proj;
    float depth;
};

struct MyVertexStructure
{
    float4 uv0;
    float4 normal;
    float4 Tangent;
    // uint color;
};

struct MyLightStructure
{
    float4 position_range;
    float4 color_intensity;
    float4 lighttype_lightDir;
    float4 matrixIDX_matrixCt_padding; // currently only used by point
    // float4x4 _DELETED; //TODO JS: delete for real
    // uint color;
};
