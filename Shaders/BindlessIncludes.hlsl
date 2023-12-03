#define USE_RW

#define LIGHTCOUNT   globals.lightcount_mode_padding_padding.r
#define VERTEXOFFSET pc.indexInfo.g
#define TEXTURESAMPLERINDEX pc.indexInfo.b
#define NORMALSAMPLERINDEX TEXTURESAMPLERINDEX +2 //TODO JS: temporary!
#define OBJECTINDEX  pc.indexInfo.a
#define SKYBOXLUTINDEX globals.lutIDX_lutSamplerIDX_padding_padding.x
#define SKYBOXLUTSAMPLERINDEX globals.lutIDX_lutSamplerIDX_padding_padding.y


struct UBO
{
    float4x4 Model;
    float4x4 NormalMat;
    float4x4 p1;
    float4x4 p2;
};

struct ShaderGlobals
{
    float4x4 view;
    float4x4 projection;
    float4 viewPos;
    float4 lightcount_mode_padding_padding;
    float4 lutIDX_lutSamplerIDX_padding_padding;
};

struct pconstant
{
    float4 indexInfo;
    float roughness;
    float metallic;
    float _f1;
    float _f2;
    float4 _1;
    float4 _2;
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
    float4 _0;
    float4 _1;
    // uint color;
};

cbuffer globals : register(b0) { ShaderGlobals globals; }
// ShaderGlobals globals;
// uniformDescriptorSets
// storageDescriptorSets
// imageDescriptorSets
// samplerDescriptorSets

[[vk::binding(0, 2)]]
Texture2D<float4> bindless_textures[];

[[vk::binding(0, 3)]]
SamplerState bindless_samplers[];

[[vk::binding(0,1)]]
#ifdef USE_RW
RWStructuredBuffer<MyVertexStructure> BufferTable;
#else
ByteAddressBuffer BufferTable;
#endif
[[vk::binding(1,1)]]
RWStructuredBuffer<MyLightStructure> lights;
[[vk::binding(2, 1)]]
RWStructuredBuffer<UBO> uboarr;


[[vk::binding(1,2)]]
TextureCube cubes[];

[[vk::binding(1,3)]]
SamplerState cubeSamplers[];

[[vk::push_constant]]
pconstant pc;



#define  DIFFUSE_INDEX  (TEXTURESAMPLERINDEX * 3) + 0
#define  SPECULAR_INDEX  (TEXTURESAMPLERINDEX * 3) + 1
#define  NORMAL_INDEX  (TEXTURESAMPLERINDEX * 3) + 2
