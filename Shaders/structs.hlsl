struct UBO
{
    float4x4 Model;
    float4x4 NormalMat;
    float4x4 p1;
    float4x4 p2;

    //Formerly push constants
    float4 indexInfo;
    float roughness;
    float metallic;
    float _f1;
    float _f2;
    float4 indexInfo_2;
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
    float4x4 mat;
    float depth;
};

struct MyVertexStructure
{
    float4 position;
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