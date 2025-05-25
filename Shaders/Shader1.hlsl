#include "BindlessIncludes.hlsl"

struct VSInput
{
    [[vk::location(0)]] float3 Position : POSITION0;
    [[vk::location(1)]] float3 Color : COLOR0;
    [[vk::location(2)]] float2 Texture_ST : TEXCOORD0;
    // [[vk::location(3)]] uint vertex_id : SV_VertexID;
};


struct FSOutput
{
    [[vk::location(0)]] float3 Color : SV_Target;
};


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
    [[vk::location(10)]] uint InstanceID : SV_InstanceID;
};

// static float2 positions[3] = {
// float2(0.0, -0.5),
// float2(0.5, 0.5),
// float2(-0.5, 0.5)
// };

VSOutput FillVertexData(VSOutput input, VertexData inputVertex)
{
    VSOutput output = input; //copy input
    output.Texture_ST = inputVertex.uv0.xy;
    output.Color = inputVertex.normal.xyz;
    output.Normal = inputVertex.normal.xyz;
    return output;


}
VSOutput FillNormalData(VSOutput input, VertexData inputVertex, float4x4 NormalMatrix, float4x4 modelMatrix)
{
    VSOutput output = input; //copy input
    
    float3x3 normalMatrix = NormalMatrix; // ?????
    float3 worldNormal = normalize(mul(normalMatrix, float4(output.Normal.x, output.Normal.y, output.Normal.z, 0.0)));
    float3 worldTangent = normalize(
        mul(modelMatrix, float4(inputVertex.Tangent.x, inputVertex.Tangent.y, inputVertex.Tangent.z, 1.0)));
    worldTangent = (worldTangent - dot(worldNormal, worldTangent) * worldNormal);
    float3 worldBinormal = (cross((worldNormal), (worldTangent))) * (inputVertex.Tangent.w);

    output.Tangent = worldTangent;
    output.Normal = worldNormal.xyz;
    output.BiTangent = worldBinormal;


    output.TBN = transpose(float3x3((worldTangent), (worldBinormal), (output.Normal)));
    return output;
}
// -spirv -T vs_6_5 -E Vert .\Shader1.hlsl -Fo .\triangle.vert.spv

VSOutput Vert(VSInput input, [[vk::builtin("BaseInstance")]] uint InstanceIndex : BaseInstance,
              uint VertexIndex : SV_VertexID)
{
#ifdef USE_RW
    VertexData myVertex = BufferTable[VertexIndex];
#else
	//Interesting buffer load perf numbers
	// https://github.com/sebbbi/perfindexInfo
	// https://github.com/microsoft/DirectXShaderCompiler/issues/2193 	
 	VertexData myVertex = BufferTable.Load<VertexData>((VERTEXOFFSET + VertexIndex) * sizeof(VertexData));
#endif
    float4 vertPos = positions[VertexIndex];
    vertPos.a = 1.0;

    Transform transform = GetTransform();
    VSOutput output = (VSOutput)0;
    output.worldPos = ObjectToWorld(vertPos);
    output.Pos =      WorldToClip(output.worldPos);

    output = FillVertexData(output, myVertex);
    output = FillNormalData(output, myVertex, transform.NormalMat, MODEL_MATRIX);

    output.InstanceID = InstanceIndex;

    output.Color = GetModelInfo().color;
    return output;
}

bool getMode()
{
    return globals.lightcount_mode_shadowct_padding.g;
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

float3 Unpack_Normals(float3 normal, float3x3 TBN)
{
    float3 normalMap = normal;
    normalMap = normalize((2.0 * normalMap) - 1.0);
    normalMap = normalize(mul(TBN, normalize(normalMap)));
    return normalMap;
}
FSOutput Frag(VSOutput input)
{
    uint InstanceIndex = 0;
    InstanceIndex = input.InstanceID;
    FSOutput output; //
    float3 diff = saturate(bindless_textures[DIFFUSE_INDEX].Sample(bindless_samplers[TEXTURESAMPLERINDEX], input.Texture_ST));
    float3 albedo = pow(diff, 2.2);
    float3 normalMap = Unpack_Normals((bindless_textures[NORMAL_INDEX].Sample(bindless_samplers[NORMALSAMPLERINDEX], input.Texture_ST)), input.TBN);

    float4 specMap = bindless_textures[SPECULAR_INDEX].Sample(bindless_samplers[TEXTURESAMPLERINDEX], input.Texture_ST);
    float metallic = specMap.a;

    float3 V = normalize(globals.eyePos - input.worldPos);
    float3 reflected = reflect(V, normalMap);

    float3 F0 = 0.04;
    F0 = lerp(F0, albedo, metallic);
    float roughness = 1.0 - specMap.r;
    float3 F = FresnelSchlickRoughness(max(dot(normalMap, V), 0.0), F0, roughness);

    float3 kS = F;
    float3 kD = 1.0 - kS;
    kD *= 1.0 - metallic;

    //
    float maxReflectionLOD = 10.0;
    float3 normalsToCubemapUVs = normalMap.xyz * float3(1, -1, 1); //TODO JS: fix root cause
    float3 irradience = cubes[0].SampleLevel(cubeSamplers[0], normalsToCubemapUVs, 1).rgb;
    float3 diffuse = irradience * albedo;


    float3 reflectedToCubemapUVs = reflected.xzy * float3(1, -1, 1); //TODO JS: fix root cause
    float3 specularcube = cubes[1].SampleLevel(cubeSamplers[1], reflectedToCubemapUVs, roughness * maxReflectionLOD).
                                   rgb;
    float2 brdfLUT = bindless_textures[SKYBOXLUTINDEX].Sample(bindless_samplers[SKYBOXLUTINDEX],
                                                              float2(max(dot(normalMap, V), 0.0), roughness)).rgb;
    float3 specularResult = specularcube * (F * brdfLUT.x + brdfLUT.y);
    float3 ambient = (kD * diffuse + specularResult);
    // output.Color =  getLighting(diff,normalMap, input.worldPos, specMap) * input.Color * irradience;
    //
    if (getMode())
    {
        output.Color = getLighting(diff, normalMap, input.worldPos, F0, roughness, metallic);
    }
    else
    {
        output.Color = ambient + getLighting(diff, normalMap, input.worldPos, F0, roughness, metallic);
    }


    output.Color *= input.Color;



    return output;
}
