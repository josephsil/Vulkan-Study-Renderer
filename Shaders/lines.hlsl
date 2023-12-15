struct VSInput
{
    [[vk::location(0)]] float3 Position : POSITION0;
    [[vk::location(1)]] float3 Color : COLOR0;
    [[vk::location(2)]] float2 Texture_ST : TEXCOORD0;
};

struct ShaderGlobals
{
    float4x4 view;
    float4x4 projection;
    float4 viewPos;
    float4 lightcount_mode_padding_padding;
    float4 lutIDX_lutSamplerIDX_padding_padding;
};
cbuffer globals : register(b0) { ShaderGlobals globals; }


struct pconstant
{
    float4x4 m; // always identity
    float4 pos1;
    float4 pos2;
    float4 color;
};

[[vk::push_constant]]
pconstant pc;



struct FSOutput
{
    [[vk::location(0)]] float3 Color : SV_Target;
};


struct VSOutput
{
    [[vk::location(0)]] float4 Pos : SV_POSITION;
    // [[vk::location(1)]] float3 Color : COLOR0;
};

VSOutput Vert(VSInput input, uint VertexIndex : SV_VertexID)
{
 

    VSOutput output = (VSOutput)0;


    float4 vertex = VertexIndex == 0 ? pc.pos1 : pc.pos2;
    float4x4 modelView = mul(globals.view, pc.m);
    float4x4 mvp = mul(globals.projection, modelView);
    output.Pos = mul(mvp, half4(vertex.xyz, 1.0));
    return output;
}


struct FSInput
{
    //[[vk::location(0)]] float4 Pos : SV_POSITION;
    [[vk::location(0)]] float3 Pos : SV_POSITION;
    [[vk::location(1)]] float3 Color : COLOR0;
};


FSOutput Frag(VSOutput input)
{
 FSOutput output = (FSOutput)0;
    output.Color =  pc.color.xyz;
    return output;
}
