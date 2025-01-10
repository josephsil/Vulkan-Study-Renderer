#define SHADOWPASS
#include "BindlessIncludes.hlsl"

struct VSInput
{
    [[vk::location(0)]] float3 Position : POSITION0;
    // [[vk::location(1)]] float3 Color : COLOR0;
    // [[vk::location(2)]] float2 Texture_ST : TEXCOORD0;
   };


struct FSOutput
{
    [[vk::location(0)]] float3 Color : SV_Target;
};



struct pconstant
{
    float unused;
    float4x4 mat;
};

[[vk::push_constant]]
pconstant pc;

struct VSOutput
{
    [[vk::location(0)]] float4 Pos : SV_POSITION;
    // [[vk::location(1)]] float3 Color : COLOR0;
    // [[vk::location(2)]] float2 Texture_ST : TEXCOORD0;
    // [[vk::location(3)]] float3 Normal : NORMAL0;
    // [[vk::location(4)]] float3 worldPos : TEXCOORD1;
    // [[vk::location(5)]] float3 Tangent : TEXCOORD2;
    // [[vk::location(6)]] float3 BiTangent : TEXCOORD3;
    // [[vk::location(7)]] float3x3 TBN : TEXCOORD4;
};

#ifdef SHADOWPASS
// #define MATRIXOFFSET pc.unused
#endif

VSOutput Vert(VSInput input,  [[vk::builtin("BaseInstance")]]  uint InstanceIndex : BaseInstance, uint VertexIndex : SV_VertexID)
{
    objectData ubo = uboarr[InstanceIndex];
    VSOutput output = (VSOutput)0;
    float4x4 viewProjection;
///
    viewProjection = pc.mat;
    
    float4x4 mvp2 = mul(viewProjection, ubo.Model);

    float4 vertPos = positions[VertexIndex];
    vertPos.a = 1.0;
    output.Pos = mul(mvp2, vertPos);
    return output;
}


struct FSInput
{
    //[[vk::location(0)]] float4 Pos : SV_POSITION;
    [[vk::location(0)]] float3 Pos : SV_POSITION;
    [[vk::location(1)]] float3 Color : COLOR0;
    [[vk::location(2)]] float2 Texture_ST : TEXCOORD0;
    [[vk::location(3)]] float3 Normal : NORMAL0;
    [[vk::location(4)]] float3 worldPos : TEXCOORD1;
    [[vk::location(5)]] float3 Tangent : TEXCOORD2;
    [[vk::location(6)]] float3 BiTangent : TEXCOORD3;
    [[vk::location(7)]] float3x3 TBN : TEXCOORD4;
};

void Frag()
{
 
}
