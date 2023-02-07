struct VSInput
{
	[[vk::location(0)]] float3 Position : POSITION0;
	[[vk::location(1)]] float3 Color : COLOR0;
};

struct VSOutput
{
	[[vk::location(0)]]  float4 Pos : SV_POSITION;
	[[vk::location(1)]] float3 Color : COLOR0;
};

static float2 positions[3] = {
	float2(0.0, -0.5),
	float2(0.5, 0.5),
	float2(-0.5, 0.5)
};

// -spirv -T vs_6_5 -E Vert .\Shader1.hlsl -Fo .\triangle.vert.spv
VSOutput Vert(VSInput input, uint VertexIndex : SV_VertexID)
{

	VSOutput output = (VSOutput)0;
	output.Pos = float4(input.Position.xy, 0.0, 1.0);
	output.Color = input.Color;
	return output;
}

struct FSInput
{
	//[[vk::location(0)]] float4 Pos : SV_POSITION;
	[[vk::location(0)]] float3 Color : COLOR0;
};

struct FSOutput
{
    [[vk::location(0)]] float3 Color : SV_Target;
};

// -spirv -T ps_6_5 -E Frag .\Shader1.hlsl -Fo .\triangle.frag.spv

FSOutput Frag(VSOutput input)
{
    FSOutput output; 

    output.Color = float4(input.Color.r , input.Color.g , 0.0, 1.0);
	return output;
}