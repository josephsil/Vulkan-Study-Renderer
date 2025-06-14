#define USE_RW

#define LIGHTCOUNT   ShaderGlobals.lightcount_padding_padding_padding.r
#define VERTEXOFFSET pc.indexInfo.g
#define TEXTURESAMPLERINDEX pc.indexInfo.b
#define NORMALSAMPLERINDEX TEXTURESAMPLERINDEX +2 //TODO JS: temporary!
#define TRANSFORMINDEX  pc.indexInfo.a

struct VSInput
{
    [[vk::location(0)]] float3 Position : POSITION0;
    // [[vk::location(1)]] float3 Color : COLOR0;
    [[vk::location(2)]] float2 Texture_ST : TEXCOORD0;
    // [[vk::location(3)]] uint vertex_id : SV_VertexID;
};

struct ObjectData
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
    float4 lightcount_padding_padding_padding;
};

struct pconstant
{
    float4 indexInfo;
    float4 _0;
    float4 _1;
    float4 _2;
};

struct VertexData
{
    float4 position;
    float4 uv0;
    float4 normal;
    float4 Tangent;
    // uint color;
};

struct LightData
{
    float4 position_range;
    float4 color_intensity;
    float4 _0;
    float4 _1;
    // uint color;
};


struct FSOutput
{
    [[vk::location(0)]] float3 Color : SV_Target;
};

cbuffer ShaderGlobals : register(b0)
{
    ShaderGlobals;
}

// ShaderGlobals ShaderGlobals;

[[vk::binding(1)]]
TextureCube bindless_textures;

[[vk::binding(2)]]
SamplerState bindless_samplers;

[[vk::binding(3)]]
#ifdef USE_RW
RWStructuredBuffer<VertexData> BufferTable;
#else
ByteAddressBuffer BufferTable;
#endif
[[vk::binding(4)]]
RWStructuredBuffer<LightData> lights;

[[vk::binding(5)]]
RWStructuredBuffer<ObjectData> uboarr;

[[vk::push_constant]]
pconstant pc;


struct VSOutput
{
    [[vk::location(0)]] float4 Pos : SV_POSITION;
    [[vk::location(1)]] float3 Color : COLOR0;
    [[vk::location(2)]] float2 Texture_ST : TEXCOORD0;
    [[vk::location(3)]] float3 Normal : NORMAL0;
    [[vk::location(4)]] float3 fragmentPos : TEXCOORD1;
    [[vk::location(5)]] float3 Tangent : TEXCOORD2;
    [[vk::location(6)]] float3 BiTangent : TEXCOORD3;
    [[vk::location(7)]] float3x3 TBN : TEXCOORD4;
};

static float2 positions[3] = {
    float2(0.0, -0.5),
    float2(0.5, 0.5),
    float2(-0.5, 0.5)
};

#define  DIFFUSE_INDEX  (TEXTURESAMPLERINDEX * 3) + 0
#define  SPECULAR_INDEX  (TEXTURESAMPLERINDEX * 3) + 1
#define  NORMAL_INDEX  (TEXTURESAMPLERINDEX * 3) + 2

// -spirv -T vs_6_5 -E Vert .\Shader1.hlsl -Fo .\triangle.vert.spv
VSOutput Vert(VSInput input, uint VertexIndex : SV_VertexID)
{
    ObjectData ubo = uboarr[TRANSFORMINDEX];
    VSOutput output = (VSOutput)0;
    output.Texture_ST = Pos;
    // Convert cubemap coordinates into Vulkan coordinate space
    output.Texture_ST.xy *= -1.0;
    output.Pos = mul(ShaderGlobals.projection, mul(ubo.Model, float4(Pos.xyz, 1.0)));
    return output;
}


struct FSInput
{
    //[[vk::location(0)]] float4 Pos : SV_POSITION;
    [[vk::location(0)]] float3 Pos : SV_POSITION;
    [[vk::location(1)]] float3 Color : COLOR0;
    [[vk::location(2)]] float2 Texture_ST : TEXCOORD0;
    [[vk::location(3)]] float3 Normal : NORMAL0;
    [[vk::location(4)]] float3 fragmentPos : TEXCOORD1;
    [[vk::location(5)]] float3 Tangent : TEXCOORD2;
    [[vk::location(6)]] float3 BiTangent : TEXCOORD3;
    [[vk::location(7)]] float3x3 TBN : TEXCOORD4;
};

// -spirv -T ps_6_5 -E Frag .\Shader1.hlsl -Fo .\triangle.frag.spv

FSOutput Frag(VSOutput input)
{
    FSOutput output;


    output.Color = bindless_textures.Sample(bindless_samplers, input.Texture_ST);

    return output;
}
