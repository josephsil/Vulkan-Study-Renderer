#define USE_RW

#define LIGHTCOUNT   pc.indexInfo.r
#define VERTEXOFFSET pc.indexInfo.g
#define TEXTUREINDEX pc.indexInfo.b
#define OBJECTINDEX  pc.indexInfo.a

struct VSInput
{
	[[vk::location(0)]] float3 Position : POSITION0;
	[[vk::location(1)]] float3 Color : COLOR0;
	[[vk::location(2)]] float2 Texture_ST : TEXCOORD0;
	// [[vk::location(3)]] uint vertex_id : SV_VertexID;
};


struct UBO
{
	float4x4 MVP;
	float4x4 Local;
	float4x4 p0;
	float4x4 p1;
};

[[vk::binding(0, 0)]]
RWStructuredBuffer<UBO> uboarr;

[[vk::binding(1)]]
Texture2D<float4> bindless_textures[];
[[vk::binding(2)]]
SamplerState bindless_samplers[];
struct pconstant
{
	float4 indexInfo;
	float4 _0;
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

[[vk::binding(3)]]
#ifdef USE_RW
RWStructuredBuffer<MyVertexStructure> BufferTable;
#else 
ByteAddressBuffer BufferTable;
#endif

[[vk::binding(4)]]
RWStructuredBuffer<MyLightStructure> lights;
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
	[[vk::location(3)]] float3 Normal : NORMAL0;
	[[vk::location(4)]] float3 fragmentPos : TEXCOORD1;
	[[vk::location(5)]] float3 Tangent : TEXCOORD2;
	[[vk::location(6)]] float3 BiTangent : TEXCOORD3;
};

static float2 positions[3] = {
	float2(0.0, -0.5),
	float2(0.5, 0.5),
	float2(-0.5, 0.5)
};

#define  DIFFUSE_INDEX  (TEXTUREINDEX * 3) + 0
#define  SPECULAR_INDEX  (TEXTUREINDEX * 3) + 1
#define  NORMAL_INDEX  (TEXTUREINDEX * 3) + 2

// -spirv -T vs_6_5 -E Vert .\Shader1.hlsl -Fo .\triangle.vert.spv
VSOutput Vert(VSInput input, uint VertexIndex : SV_VertexID)
{

	#ifdef USE_RW
	MyVertexStructure myVertex = BufferTable[VertexIndex + VERTEXOFFSET];
	#else 
	//Interesting buffer load perf numbers
	// https://github.com/sebbbi/perfindexInfo
	// https://github.com/microsoft/DirectXShaderCompiler/issues/2193 	
 	MyVertexStructure myVertex = BufferTable.Load<MyVertexStructure>((VERTEXOFFSET + VertexIndex) * sizeof(MyVertexStructure));
	#endif
	UBO ubo = uboarr[OBJECTINDEX];
	VSOutput output = (VSOutput)0;
	output.Pos = mul(ubo.MVP, half4(myVertex.position.xyz, 1.0));
	output.Texture_ST = myVertex.uv0.xy;
	output.Color = myVertex.normal.xyz;
	output.Normal = myVertex.normal.xyz;
	output.Normal = mul(ubo.Local, half4(myVertex.normal.xyz, 0.0) );
	output.fragmentPos = mul(ubo.Local, half4(myVertex.position.xyz, 1.0) );

	//Not sure if the mul here is correct??
	output.Tangent =  mul(ubo.Local, myVertex.Tangent.xyz);
	output.BiTangent =  mul(ubo.Local,(1 * cross(myVertex.normal, myVertex.Tangent.xyz)));

	// output.Pos = float4(positions[2 - (VertexIndex % 2)],1,1);
	// output.Color = myVertex.uv0.xy;
	return output;
}



struct FSInput
{
	//[[vk::location(0)]] float4 Pos : SV_POSITION;
	[[vk::location(0)]] float3 Pos : SV_POSITION;
	[[vk::location(1)]] float3 Color : COLOR0;
	[[vk::location(2)]] float2 Texture_ST : TEXCOORD0;
	[[vk::location(3)]] float2 Normal : NORMAL0;
	[[vk::location(4)]] float3 fragmentPos : TEXCOORD1;
	[[vk::location(5)]] float3 Tangent : TEXCOORD2;
	[[vk::location(6)]] float3 BiTangent : TEXCOORD3;
};

float3 getLighting(float3 incolor, float3 inNormal, float3 FragPos)
{
	float3 lightContribution = float3(0,0,0);
	for(int i = 0; i < LIGHTCOUNT; i++)
	{
		MyLightStructure light = lights[i];
		float3 lightPos = light.position_range.xyz;
		float lightRange = light.position_range.a;
		float3 lightColor = light.color_intensity.xyz * light.color_intensity.a;
		float3 lightToFrag   = lightPos - FragPos;
		float3 lightDir_norm = normalize(lightToFrag);
		float lightDistance = length(lightToFrag);
		// vec3 viewDir    = normalize(viewPos - FragPos);
		// vec3 halfwayDir = normalize(lightDir + viewDir);


   		float Attenuation =  saturate(lightRange / lightDistance);

		Attenuation =  1.0 / (lightDistance * lightDistance);
		// Attenuation = 3;
		float diff = saturate(dot(inNormal, lightDir_norm));
		lightContribution += ((lightColor * diff) * Attenuation );
	}
		

		return incolor * lightContribution;

}





// -spirv -T ps_6_5 -E Frag .\Shader1.hlsl -Fo .\triangle.frag.spv

FSOutput Frag(VSOutput input)
{
	FSOutput output;

	float3 diff  = saturate(bindless_textures[DIFFUSE_INDEX].Sample(bindless_samplers[TEXTUREINDEX], input.Texture_ST) + 0.2) ;
	float3 normalMap  = bindless_textures[NORMAL_INDEX].Sample(bindless_samplers[TEXTUREINDEX], input.Texture_ST);
	float3 normal =  normalize( normalMap.x * input.Tangent + normalMap.y * input.BiTangent + normalMap.z * input.Normal );
	output.Color =  (getLighting(diff, input.Normal * normalize((normalMap * 2.0 )- 1.0), input.fragmentPos));
	return output;
}