#define SHADOWPASS
#include "BindlessIncludes.hlsl"

struct VSInput
{
    [[vk::location(0)]] float3 Position : POSITION0;
    [[vk::location(1)]] float3 Color : COLOR0;
    [[vk::location(2)]] float2 Texture_ST : TEXCOORD0;
   };


struct FSOutput
{
    [[vk::location(0)]] float3 Color : SV_Target;
};


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

VSOutput Vert(VSInput input, uint VertexIndex : SV_VertexID)
{
    bool mode = globals.lightcount_mode_shadowct_padding.g;
#ifdef USE_RW
    MyVertexStructure myVertex = BufferTable[VertexIndex + VERTEXOFFSET];
#else
 	MyVertexStructure myVertex = BufferTable.Load<MyVertexStructure>((VERTEXOFFSET + VertexIndex) * sizeof(MyVertexStructure));
#endif
    UBO ubo = uboarr[OBJECTINDEX];
    VSOutput output = (VSOutput)0;
    MyLightStructure light = lights[LIGHTINDEX];
    float4x4 viewProjection;
    if (getLightType(light) == LIGHT_POINT)
    {
        viewProjection = shadowMatrices[MATRIXOFFSET];
    }
    else
    {
        viewProjection = light.matrixViewProjeciton;
    }
    float4x4 mvp2 = mul(viewProjection, ubo.Model);
    
    output.Pos = mul(mvp2, half4(myVertex.position.xyz, 1.0));
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
