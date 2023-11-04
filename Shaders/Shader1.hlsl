#define USE_RW

#define LIGHTCOUNT   globals.lightcount_padding_padding_padding.r
#define VERTEXOFFSET pc.indexInfo.g
#define TEXTURESAMPLERINDEX pc.indexInfo.b
#define NORMALSAMPLERINDEX TEXTURESAMPLERINDEX +2 //TODO JS: temporary!
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

	


struct FSOutput
{
	[[vk::location(0)]] float3 Color : SV_Target;
};

cbuffer globals : register(b0) { ShaderGlobals globals; }
// ShaderGlobals globals;

[[vk::binding(1)]]
Texture2D<float4> bindless_textures[];

[[vk::binding(2)]]
SamplerState bindless_samplers[];

[[vk::binding(3)]]
#ifdef USE_RW
RWStructuredBuffer<MyVertexStructure> BufferTable;
#else 
ByteAddressBuffer BufferTable;
#endif
[[vk::binding(4)]]
RWStructuredBuffer<MyLightStructure> lights;

[[vk::binding(5)]]
RWStructuredBuffer<UBO> uboarr;

[[vk::binding(6)]]
TextureCube cube;

[[vk::binding(7)]]
SamplerState cubeSampler;

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
	float4x4 modelView = mul( globals.view, ubo.Model);
	float4x4 mvp = mul(globals.projection,modelView);
	output.Pos = mul(mvp, half4(myVertex.position.xyz, 1.0));
	output.Texture_ST = myVertex.uv0.xy;
	output.Color = myVertex.normal.xyz;
	output.Normal = myVertex.normal.xyz;
	output.fragmentPos = mul(ubo.Model, half4(myVertex.position.xyz, 1.0) );

	float3x3 normalMatrix = ubo.NormalMat; // TODO: move
	//bitangent = fSign * cross(vN, tangent);
	//Not sure if the mul here is correct? would need something baked
	float3 worldNormal = normalize(mul(normalMatrix, float4(myVertex.normal.x, myVertex.normal.y, myVertex.normal.z,  0.0) ));
	float3 worldTangent = normalize(mul(ubo.Model, float4(myVertex.Tangent.x,myVertex.Tangent.y,myVertex.Tangent.z,1.0)));
	worldTangent = (worldTangent - dot(worldNormal, worldTangent) * worldNormal);
	float3 worldBinormal = (cross( (worldNormal), (worldTangent))) * ( myVertex.Tangent.w )  ;
	
	output.Tangent = worldTangent;
	output.Normal = worldNormal;
	output.BiTangent = worldBinormal;

    output.TBN = float3x3((worldTangent), (worldBinormal), (worldNormal));

//   float3 normalW = normalize(float3(u_NormalMatrix * vec4(a_Normal.xyz, 0.0)));
//   float3 tangentW = normalize(float3(u_ModelMatrix * vec4(a_Tangent.xyz, 0.0)));
//   float3 bitangentW = cross(normalW, tangentW) * a_Tangent.w;
//   v_TBN = mat3(tangentW, bitangentW, normalW);


	output.Color = 1.0;
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

float3 getLighting(float3 incolor, float3 inNormal, float3 FragPos, float3 specMap)
{
	float3 lightContribution = float3(0,0,0);
	for(int i = 0; i < LIGHTCOUNT; i++)
	{
		MyLightStructure light = lights[i];
		float3 lightPos = light.position_range.xyz;
		float lightRange = light.position_range.a;
		float3 lightColor = light.color_intensity.xyz * light.color_intensity.a;
		float3 lightToFrag   = lightPos - FragPos;
		float3 lightDir = normalize(lightToFrag);
		float lightDistance = length(lightToFrag);
		float3 viewDir    = normalize( globals.viewPos - FragPos );
		float3 halfwayDir = normalize(lightDir + viewDir);

		float3 reflectDir = normalize(reflect(-lightDir, inNormal));

   		float Attenuation =  saturate(lightRange / lightDistance);

		Attenuation =  1.0 / (lightDistance * lightDistance);
		// Attenuation = 3;
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), 16.0) ;

//  spec = 0;
		float diff = saturate(dot(inNormal, lightDir));
		lightContribution += ((lightColor * diff) * Attenuation ) + ((spec * lightColor));
	}
		

		return incolor * lightContribution;

}





// -spirv -T ps_6_5 -E Frag .\Shader1.hlsl -Fo .\triangle.frag.spv

FSOutput Frag(VSOutput input)
{
	FSOutput output;

	float3 diff  = saturate(bindless_textures[DIFFUSE_INDEX].Sample(bindless_samplers[TEXTURESAMPLERINDEX], input.Texture_ST)) ;
	float3 normalMap  = (bindless_textures[NORMAL_INDEX].Sample(bindless_samplers[NORMALSAMPLERINDEX], input.Texture_ST));
	float3 specMap  = bindless_textures[SPECULAR_INDEX].Sample(bindless_samplers[TEXTURESAMPLERINDEX], input.Texture_ST);
	
 	normalMap = normalize(mul(normalize(((2.0 * normalMap) - 1.0)), input.TBN));

	float4 envColor = cube.Sample(cubeSampler,  float3(input.Normal.x, input.Normal.y, input.Normal.z), 1);
	// diff = float3(1.0,1.0,1.0);
	output.Color =  (getLighting(diff,normalMap, input.fragmentPos, specMap)) * input.Color;

		output.Color =envColor;
	// output.Color = input.TBN[0];
	return output;
}