#define USE_RW
#include "structs.hlsl"
#include "GeneralIncludes.hlsl"
#include "ObjectDataMacros.hlsl"
#define LIGHT_DIR 0
#define LIGHT_POINT 1
#define LIGHT_SPOT 2



[[vk::binding(0, 0)]]
Texture2D<float4> bindless_textures[];
[[vk::binding(1,0)]]
TextureCube cubes[];
[[vk::binding(2, 0)]]
SamplerState bindless_samplers[];
[[vk::binding(3,0)]]
SamplerState cubeSamplers[];
[[vk::binding(4,0)]]
#ifdef USE_RW
RWStructuredBuffer<VertexData> BufferTable;
#else
ByteAddressBuffer BufferTable;
#endif


//[[vk::binding(0, 1)]]
cbuffer ShaderGlobals : register(b0, space1) { ShaderGlobals ShaderGlobals; }
[[vk::binding(1, 1)]]
Texture2DArray<float4> shadowmap[];
[[vk::binding(1, 1)]]
TextureCube shadowmapCube[];
[[vk::binding(2, 1)]]
SamplerState shadowmapSampler[];
[[vk::binding(3,1)]]
RWStructuredBuffer<LightData> lights;
[[vk::binding(4, 1)]]
RWStructuredBuffer<ObjectData> PerObjectData;
[[vk::binding(5, 1)]]
RWStructuredBuffer<perShadowData> shadowMatrices;
[[vk::binding(5, 0)]]
RWStructuredBuffer<float4> positions;
[[vk::binding(6, 1)]]
RWStructuredBuffer<Transform> Transforms;

#define  DIFFUSE_INDEX  PerObjectData[InstanceIndex].props.textureIndexInfo.r
#define  SPECULAR_INDEX  PerObjectData[InstanceIndex].props.textureIndexInfo.g
#define  NORMAL_INDEX PerObjectData[InstanceIndex].props.textureIndexInfo.b

float3 GetSpotLightDir(LightData light)
{
    return light.lighttype_lightDir.yzw; //what!
}

int GetLightType(LightData light)
{
    return light.lighttype_lightDir.x;
}

perShadowData GetShadowDataForLight(LightData light, uint shadowOffset)
{
    uint index = light.shadowOffset + shadowOffset;
    return shadowMatrices[index];
}

float4x4 GetWorldToClipForShadow(perShadowData data)
{
    return  mul(data.projMatrix,data.viewMatrix);
}
int getShadowMatrixIndex(LightData light)
{
    return light.shadowOffset;
}

int getShadowMatrixCount(LightData light)
{
    return light.shadowCount;
}

float3 FresnelSchlickRoughness(float cosTheta, float3 F0, float roughness)
{
    return F0 + (max(float3(1.0 - roughness, 1.0 - roughness, 1.0 - roughness), F0) - F0) * pow(
        clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float3 fresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}


float DistributionGGX(float3 N, float3 H, float roughness)
{
    // float PI = 3.14159265359;

    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return num / denom;
}

float2 pointShadowDirection(int idx)
{
    return float2((idx < 3 ? 0 : 0.5), (0.333 * (idx % 3)));
}

float uvToPointShadow(int idx, float2 uv)
{
    float2 offset = pointShadowDirection(idx);
    uv *= float2(0.5, 0.33);
    uv += offset;
    return uv;
}

float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}



//I copied this from somewhere and don't totally understand it!
float WorldSpacePositionToPointLightNDCDepth(float3 Vec)
{
    float3 AbsVec = abs(Vec);
    float LocalZcomp = max(AbsVec.x, max(AbsVec.y, AbsVec.z));

    const float FAR = POINT_LIGHT_FAR_PLANE;
    const float NEAR = POINT_LIGHT_NEAR_PLANE;
    float NormZComp = (FAR + NEAR) / (FAR - NEAR) - (2.0 * FAR * NEAR) / (FAR - NEAR) / LocalZcomp;
    return (NormZComp + 1.0) * 0.5;
}


 int findCascadeLevel(int lightIndex, float3 worldPixel)
 {
     float4 fragPosViewSpace = mul(ShaderGlobals.viewMatrix, float4(worldPixel, 1.0));
     float depthValue = fragPosViewSpace.z;
     int cascadeLevel = 0;
     for (int i = 0; i < CASCADE_CT; i++) //6 == cascade count
     {
         if (depthValue > shadowMatrices[lightIndex + i].depth)
         {
             cascadeLevel = i;
             break;
         }
     }

     return cascadeLevel;
 }
static const int POISSON_SAMPLECOUNT = 12;
float SampleSoftShadow(Texture2DArray<float4> Texture, float fragDepth, float3 shadowUV)
{
    //Poisson lookup
    //Version from https://web.engr.oregonstate.edu/~mjb/cs519/Projects/Papers/ShaderTricks.pdf -- placheolder
    float2 vTexelSize = float2(1.0 / SHADOW_MAP_SIZE, 1.0 / SHADOW_MAP_SIZE);
    float2 vTaps[POISSON_SAMPLECOUNT] = {
        float2(-0.326212, -0.40581), float2(-0.840144, -0.07358),
        float2(-0.695914, 0.457137), float2(-0.203345, 0.620716),
        float2(0.96234, -0.194983), float2(0.473434, -0.480026),
        float2(0.519456, 0.767022), float2(0.185461, -0.893124),
        float2(0.507431, 0.064425), float2(0.89642, 0.412458),
        float2(-0.32194, -0.932615), float2(-0.791559, -0.59771)
    };

    float cSampleAccum = 0;
    // Take a sample at the disc’s center
    cSampleAccum += Texture.Sample(shadowmapSampler[0], shadowUV.xyz) <= fragDepth;
    // Take 12 samples in disc
    for (int nTapIndex = 0; nTapIndex < POISSON_SAMPLECOUNT; nTapIndex++)
    {
        float2 vTapCoord = vTexelSize * vTaps[nTapIndex] * 1.5;
        float3 uvOffset = float3(vTapCoord, 0.0);
        // Accumulate samples
        cSampleAccum += (Texture.Sample(shadowmapSampler[0], (shadowUV.xyz + uvOffset)).r <= fragDepth);
    }
    return cSampleAccum;
}

float3 getShadow(int index, float3 fragWorldPos)
{
    LightData light = lights[index];
    if (GetLightType(light) == LIGHT_POINT)
    {
        float3 fragToLight = fragWorldPos - light.position_range.xyz;
        float distLightSpace = WorldSpacePositionToPointLightNDCDepth(light.position_range.xyz - fragWorldPos);
        //ShadowMapCube is indexed by light index, rather than shadowmap index like the sampler the other light types use.
        //There's high risk of a bug here with how I stride lights.
        float3 shadow = shadowmapCube[index].SampleLevel(cubeSamplers[0], fragToLight, 0.0).r;
        return distLightSpace > (shadow.r);
    }
    if (GetLightType(light) == LIGHT_SPOT)
    {
        int ARRAY_INDEX = 0;
        float4x4 lightMat = GetWorldToClipForShadow(GetShadowDataForLight(light, ARRAY_INDEX));
        float4 fragClipSpace = mul(lightMat, float4(fragWorldPos, 1.0));
        float3 fragNDC = ClipToNDC(fragClipSpace);
        float2 shadowUV = NDCToUV(fragNDC);

        return shadowmap[light.shadowOffset].Sample(shadowmapSampler[0], float3(shadowUV, ARRAY_INDEX)).r <= fragNDC.z;
    }
    if (GetLightType(light) == LIGHT_DIR)
    {
        int lightIndex = getShadowMatrixIndex(light);
        int cascadeLevel = findCascadeLevel(lightIndex, fragWorldPos);
        float4 fragClipSpace = mul(GetWorldToClipForShadow(GetShadowDataForLight(light, lightIndex + cascadeLevel)), float4(fragWorldPos, 1.0));
        float3 fragNDC = ClipToNDC(fragClipSpace);
        float2 shadowUV = NDCToUV(fragNDC);
 
        float cSampleAccum = SampleSoftShadow(shadowmap[light.shadowOffset],  fragNDC.z, float3(shadowUV.xy, cascadeLevel));
      
        return saturate((cSampleAccum) / (POISSON_SAMPLECOUNT + 1));
    }
    return -1;
}
float3 GetLightPosition(LightData l)
{
    return l.position_range.xyz;
}
float3 GetDirLightDirection(LightData l)
{
    return l.position_range.xyz;
}
float GetDirLightMaxRadius(LightData l)
{
    return l.position_range.a;
}
float3 GetLightDir(LightData light, float3 fragPos)
{

    if (GetLightType(light) == LIGHT_DIR)
    {
        return GetDirLightDirection(light);
    }
    
    else //(getLightType(light) == LIGHT_POINT || getLightType(light) == LIGHT_SPOT)
    {
        return normalize(GetLightPosition(light) - (fragPos));
    }

}

float3 lightContribution(float3 halfwayDir, float3 viewDir, float3 lightDir, float3 surfaceNormal, float3 lightRadiance, float3 albedo, float metallic, float roughness)
{
    float3 F0 = 0.04;
    F0 = lerp(F0, albedo, metallic);
    // float PI = 3.14159265359;
    float3 Fresnel = fresnelSchlick(max(dot(halfwayDir, viewDir), 0.0), F0);
    float NDF = DistributionGGX(surfaceNormal, halfwayDir, roughness);
    float G = GeometrySmith(surfaceNormal, viewDir, lightDir, roughness);
    float3 numerator = NDF * G * Fresnel;
    float denominator = 4.0 * max(dot(surfaceNormal, viewDir), 0.0) * max(dot(surfaceNormal, lightDir), 0.0) + 0.0001;
    float3 specular = numerator / denominator;
    float3 kS = Fresnel;
    float3 kD = 3.0 - kS;

    kD *= 1.0 - metallic;
    float NdotL = max(dot(surfaceNormal, lightDir), 0.0);
    float3 lightAdd = (kD * albedo / PI + specular) * lightRadiance * NdotL;
    return lightAdd;
}
float3 getLighting(float3 albedo, float3 inNormal, float3 fragWorldPos, float3 F0, float3 roughness, float metallic)
{
    float3 viewDir = normalize(ShaderGlobals.eyePos - fragWorldPos);
    float3 lightResult = 0;
    for (int i = 0; i < LIGHTCOUNT; i++)
    {
        LightData light = lights[i];
        float3 lightColor = light.color_intensity.xyz * light.color_intensity.a;
        float3 radiance = lightColor;
        
        float3 lightPos = GetLightPosition(light);
        float3 lightDir = GetLightDir(light,  fragWorldPos);
        float lightDistance = pow(length(lightPos - fragWorldPos), 2);
        float3 halfwayDir = normalize(lightDir + viewDir);

        int lightType = GetLightType(light);
        if (lightType == LIGHT_POINT || lightType == LIGHT_SPOT)
        {
            float attenuation = 1.0 / (lightDistance * lightDistance);
            radiance *= attenuation;
        }
        if (lightType == LIGHT_SPOT)
        {
            float3 spotlightDir = GetSpotLightDir(light);
            float theta = dot(normalize(lightDir), normalize(-spotlightDir));

            float cosCutoff = (1.0 - (GetDirLightMaxRadius(light)) / 180);
            float softFactor = 1.2; //TODO JS: paramaterize 
            float cosCutoffOuter = (1.0 - (GetDirLightMaxRadius(light) * softFactor) / 180);
            float epsilon = cosCutoff - cosCutoffOuter;

            radiance *= clamp((theta - cosCutoffOuter) / epsilon, 0.0, 1.0);
        }

        float3 lightAdd =  lightContribution(halfwayDir, viewDir, lightDir, inNormal, radiance, albedo, metallic, roughness);
        //Shadow:
        if (i < SHADOWCOUNT) //TODO JS: pass in max shadow casters?
        {
            lightAdd *=  getShadow(i, fragWorldPos);
        }
        lightResult += lightAdd;
    }


    return lightResult;
}

float3 very_approximate_srgb_to_linear(float3 input)
{
    return input*input;
}

//
float3x3 calculateNormal(float3 Normal, float3 Tangent)
{
    float3 N = normalize(Normal);
    float3 T = normalize(Tangent);
    float3 B = normalize(cross(N, T));
    float3x3 TBN = transpose(float3x3(T, B, N));

    return TBN;
}