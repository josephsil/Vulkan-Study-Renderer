#define USE_RW

#define LIGHTCOUNT   globals.lightcount_padding_padding_padding.r
#define VERTEXOFFSET pc.indexInfo.g
#define TEXTURESAMPLERINDEX pc.indexInfo.b
#define NORMALSAMPLERINDEX TEXTURESAMPLERINDEX +2 //TODO JS: temporary!
#define OBJECTINDEX  pc.indexInfo.a
#define SKYBOXLUTINDEX globals.lutIDX_lutSamplerIDX_padding_padding.x
#define SKYBOXLUTSAMPLERINDEX globals.lutIDX_lutSamplerIDX_padding_padding.y

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
	float4 lutIDX_lutSamplerIDX_padding_padding;
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
TextureCube cubes[];

[[vk::binding(7)]]
SamplerState cubeSamplers[];

[[vk::push_constant]]
pconstant pc;



struct VSOutput
{
	[[vk::location(0)]] float4 Pos : SV_POSITION;
	[[vk::location(1)]] float3 Color : COLOR0;
	[[vk::location(2)]] float2 Texture_ST : TEXCOORD0;
	[[vk::location(3)]] float3 Normal : NORMAL0;
	[[vk::location(4)]] float3 worldPos : TEXCOORD1;
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
	output.worldPos = mul(ubo.Model, half4(myVertex.position.xyz, 1.0) );

	float3x3 normalMatrix = ubo.NormalMat; // ?????
	//bitangent = fSign * cross(vN, tangent);
	//Not sure if the mul here is correct? would need something baked
	float3 worldNormal = normalize(mul((float3x3)normalMatrix, float4(output.Normal.x, output.Normal.y, output.Normal.z,  0.0) ));
	float3 worldTangent = normalize(mul(ubo.Model, float4(myVertex.Tangent.x,myVertex.Tangent.y,myVertex.Tangent.z,1.0)));
	worldTangent = (worldTangent - dot(worldNormal, worldTangent) * worldNormal);
	float3 worldBinormal = (cross( (worldNormal), (worldTangent))) * ( myVertex.Tangent.w )  ;
	
	output.Tangent = worldTangent;
	output.Normal = worldNormal.xyz;
	output.BiTangent = worldBinormal;

    output.TBN = transpose(float3x3((worldTangent), (worldBinormal), (output.Normal)));

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
	[[vk::location(4)]] float3 worldPos : TEXCOORD1;
	[[vk::location(5)]] float3 Tangent : TEXCOORD2;
	[[vk::location(6)]] float3 BiTangent : TEXCOORD3;
	[[vk::location(7)]] float3x3 TBN : TEXCOORD4;
};

float3x3 calculateNormal(FSInput input)
{
	// float3 tangentNormal = normalMapTexture.Sample(normalMapSampler, input.UV).xyz * 2.0 - 1.0;

	float3 N = normalize(input.Normal);
	float3 T = normalize(input.Tangent);
	float3 B = normalize(cross(N, T));
	float3x3 TBN = transpose(float3x3(T, B, N));

	return TBN;
}

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


float3  FresnelSchlickRoughness(float cosTheta, float3 F0, float roughness)
{
    return F0 + (max(float3(1.0 - roughness,1.0 - roughness,1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}   


// -spirv -T ps_6_5 -E Frag .\Shader1.hlsl -Fo .\triangle.frag.spv

FSOutput Frag(VSOutput input)
{
	FSOutput output;

	float3 diff  = saturate(bindless_textures[DIFFUSE_INDEX].Sample(bindless_samplers[TEXTURESAMPLERINDEX], input.Texture_ST)) ;
	float3 albedo = pow(diff, 2.2);
	float3 normalMap  = (bindless_textures[NORMAL_INDEX].Sample(bindless_samplers[NORMALSAMPLERINDEX], input.Texture_ST));
	float3 specMap  = bindless_textures[SPECULAR_INDEX].Sample(bindless_samplers[TEXTURESAMPLERINDEX], input.Texture_ST);
	float metallic = 1.0;

albedo = 0.5;

	
	normalMap = normalize(mul(input.TBN, ((2.0 * float3(0,0,1)) - 1.0)));
	normalMap = input.Normal;
	
	
	float3 V    = normalize( globals.viewPos - input.worldPos);
	float3 reflected = reflect(V, normalMap);

	float3 F0 = 0.04; // TODO: f0 for metallic
	F0      = lerp(F0, albedo, metallic);
	float roughness = 0.1;
	float3 F = FresnelSchlickRoughness(max(dot(normalMap, V), 0.0), F0, roughness);

	float3 kS = F;
	float3 kD = 1.0 - kS;
	kD *= 1.0 - metallic;


	float maxReflectionLOD = 6.0;
	float3 fixedNormals = normalMap.xyz  * float3(1,-1,1);;
	float3 irradience =  pow(cubes[0].Sample(cubeSamplers[0],  fixedNormals, 1).rgb, 2.2) ;
	float3 diffuse = irradience * albedo;
	float3 fixedReflected = reflected.xzy * float3(1,-1,1);
	float3 specularcube = pow(cubes[0].Sample(cubeSamplers[1],  fixedReflected,  roughness * maxReflectionLOD).rgb, 2.2) ;
	float2 cubeLUT = bindless_textures[SKYBOXLUTINDEX].Sample(bindless_samplers[SKYBOXLUTINDEX], float2(max(dot(normalMap,V), 0.0),roughness)).rgb;
	float3 specularResult = specularcube * (F* cubeLUT.x + cubeLUT.y);
	float3 ambient = (kD * diffuse  + specularResult);
	output.Color =  (getLighting(diff,normalMap, input.worldPos, specMap)) * input.Color * irradience;

	output.Color =   ambient;
	// output.Color = reflected;
	return output;
}