struct VSInput
{
	[[vk::location(0)]] float3 Position : POSITION0;
	[[vk::location(1)]] float3 Color : COLOR0;
	[[vk::location(2)]] float2 Texture_ST : TEXCOORD0;
	// [[vk::location(3)]] uint vertex_id : SV_VertexID;
};


struct UBO
{
	float4x4 model;
	float4x4 view;
	float4x4 proj;
	float4x4 padding;
};

[[vk::binding(0, 0)]]
RWStructuredBuffer<UBO> uboarr;

[[vk::binding(1)]]
Texture2D<float4> bindless_textures[];
[[vk::binding(2)]]
SamplerState bindless_samplers[];
struct pconstant
{
	float4 test;
};

struct MyVertexStructure
{
    float4 position;
    float4 uv0;
	float4 _0;
    float4 _1;
    // uint color;
};

[[vk::binding(3)]]
RWStructuredBuffer<MyVertexStructure> BufferTable;

struct FSOutput
{
	[[vk::location(0)]] float3 Color : SV_Target;
};


[[vk::push_constant]]
pconstant pc;

struct VSOutput
{
	[[vk::location(0)]] float4 Pos : SV_POSITION;
	[[vk::location(1)]] float3 Color : COLOR0;
	[[vk::location(2)]] float2 Texture_ST : TEXCOORD0;
};

static float2 positions[3] = {
	float2(0.0, -0.5),
	float2(0.5, 0.5),
	float2(-0.5, 0.5)
};

// -spirv -T vs_6_5 -E Vert .\Shader1.hlsl -Fo .\triangle.vert.spv
VSOutput Vert(VSInput input, uint VertexIndex : SV_VertexID)
{

	int vertOffset = pc.test.g;
 	MyVertexStructure myVertex = BufferTable[VertexIndex + vertOffset];
	int idx = pc.test.a;
	UBO ubo = uboarr[idx];
	VSOutput output = (VSOutput)0;
	output.Pos = mul(mul(mul(ubo.proj, ubo.view), ubo.model), half4(myVertex.position.xyz, 1.0));
	// output.Color = input.Color;
	output.Texture_ST = myVertex.uv0.xy;

	// output.Pos = float4(positions[2 - (VertexIndex % 2)],1,1);
	// output.Color = myVertex.uv0.xy;
	return output;
}

// [[vk::combinedImageSampler]] [[vk::binding(1)]]
// Texture2D<float4> myTexture;
// [[vk::combinedImageSampler]] [[vk::binding(1)]]
// SamplerState mySampler;

struct FSInput
{
	//[[vk::location(0)]] float4 Pos : SV_POSITION;
	[[vk::location(0)]] float3 Pos : SV_POSITION;
	[[vk::location(1)]] float3 Color : COLOR0;
	[[vk::location(2)]] float2 Texture_ST : TEXCOORD0;
};





// -spirv -T ps_6_5 -E Frag .\Shader1.hlsl -Fo .\triangle.frag.spv

FSOutput Frag(VSOutput input)
{
	FSOutput output;

	output.Color = saturate(bindless_textures[pc.test.b].Sample(bindless_samplers[pc.test.b], input.Texture_ST) + 0.2) ;
	// output.Color = input.Color;
	return output;
}