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
    // printf("My float is %d\n", InstanceIndex);
    //
    Transform transform = GetTransform();
    VSOutput output = (VSOutput)0;
    //
    float4x4 modelView = mul(ShaderGlobals.viewMatrix, GetTransform().Model);
    float4x4 mvp = mul(ShaderGlobals.projMatrix, modelView);
    output.Pos = mul(mvp, vertPos);
    output.Texture_ST = myVertex.uv0.xy;
    output.Color = myVertex.normal.xyz;
    output.Normal = myVertex.normal.xyz;
    output.worldPos = mul(transform.Model, vertPos);

    float3x3 normalMatrix = transform.NormalMat; // ?????
    //bitangent = fSign * cross(vN, tangent);
    //Not sure if the mul here is correct? would need something baked
    float3 worldNormal = normalize(mul(normalMatrix, float4(output.Normal.x, output.Normal.y, output.Normal.z, 0.0)));
    float3 worldTangent = normalize(
        mul(transform.Model, float4(myVertex.Tangent.x, myVertex.Tangent.y, myVertex.Tangent.z, 1.0)));
    worldTangent = (worldTangent - dot(worldNormal, worldTangent) * worldNormal);
    float3 worldBinormal = (cross((worldNormal), (worldTangent))) * (myVertex.Tangent.w);

    output.Tangent = worldTangent;
    output.Normal = worldNormal.xyz;
    output.BiTangent = worldBinormal;


    output.TBN = transpose(float3x3((worldTangent), (worldBinormal), (output.Normal)));

    //   float3 normalW = normalize(float3(u_NormalMatrix * vec4(a_Normal.xyz, 0.0)));
    //   float3 tangentW = normalize(float3(u_ModelMatrix * vec4(a_Tangent.xyz, 0.0)));
    //   float3 bitangentW = cross(normalW, tangentW) * a_Tangent.w;
    //   v_TBN = mat3(tangentW, bitangentW, normalW);

    output.InstanceID = InstanceIndex;


    output.Color = GetModelInfo().color;
    return output;
}

bool getMode()
{
    return ShaderGlobals.lightcount_mode_shadowct_padding.g;
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


//
float3x3 calculateNormal(FSInput input)
{
    // float3 tangentNormal = normalMapTexture.Sample(normalMapSampler, input.UV).xyz * 2.0 - 1.0;

    float3 N = normalize(input.Normal);
    float3 T = normalize(input.Tangent);
    float3 B = normalize(cross(N, T));
    float3x3 TBN = transpose(float3x3(T, B, N));

    return TBN;
}

//
// -spirv -T ps_6_5 -E Frag .\Shader1.hlsl -Fo .\triangle.frag.spv

FSOutput Frag(VSOutput input)
{
    uint InstanceIndex = 0;
    InstanceIndex = input.InstanceID;
    FSOutput output; //
    float3 diff = saturate(
        bindless_textures[DIFFUSE_INDEX].Sample(bindless_samplers[TEXTURESAMPLERINDEX], input.Texture_ST));
    float3 albedo = pow(diff, 2.2);
    float3 normalMap = (bindless_textures[NORMAL_INDEX].
        Sample(bindless_samplers[NORMALSAMPLERINDEX], input.Texture_ST));

    float4 specMap = bindless_textures[SPECULAR_INDEX].Sample(bindless_samplers[TEXTURESAMPLERINDEX], input.Texture_ST);
    float metallic = specMap.a;
    // albedo = 0.33;

    normalMap = normalize((2.0 * normalMap) - 1.0);

    normalMap = normalize(mul(input.TBN, normalize(normalMap)));

    float3 V = normalize(ShaderGlobals.eyePos - input.worldPos);
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


    //
    output.Color *= input.Color;


    // output.Color *= float3(1.0f, 0.5f, 0.5f);
    // int cascadelevel = findCascadeLevel(0, input.worldPos);
    // if (cascadelevel == 0)
    // {
    //     output.Color += (float3(1,0,0) *0.05f*0.5);
    // }
    // else if (cascadelevel == 1)
    // {
    //     output.Color += (float3(1,0,0) *0.05f*0.3);
    // }
    // else if (cascadelevel ==2)
    // {
    //     output.Color += (float3(0,1,0) *0.05f*0.5);
    // }
    // else if (cascadelevel == 3)
    // {
    //     output.Color += (float3(0,0,1) *0.05f*0.3);
    // }
    // else if (cascadelevel == 4)
    // {
    //     output.Color += (float3(0,0,1) *0.05f*0.5);
    // }
    // else if (cascadelevel == 5)
    // {
    //     output.Color += (float3(0,0,1) *0.05f*0.3);
    // }


    return output;
}
