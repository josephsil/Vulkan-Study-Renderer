#define USE_RW
#include "structs.hlsl"
#define LIGHT_DIR 0
#define LIGHT_POINT 1
#define LIGHT_SPOT 2
#define LIGHTCOUNT   globals.lightcount_mode_shadowct_padding.r
#define VERTEXOFFSET perObjectData[InstanceIndex].props.indexInfo.g
#define TEXTURESAMPLERINDEX  perObjectData[InstanceIndex].props.textureIndexInfo.g
#define NORMALSAMPLERINDEX  perObjectData[InstanceIndex].props.textureIndexInfo.b //TODO JS: temporary!
#define TRANSFORMINDEX  perObjectData[InstanceIndex].props.indexInfo.a
#define GetTransform()  transforms[perObjectData[InstanceIndex].props.indexInfo.a]
#define GetModelInfo() perObjectData[InstanceIndex].props
#define SKYBOXLUTINDEX globals.lutIDX_lutSamplerIDX_padding_padding.x
#define SKYBOXLUTSAMPLERINDEX globals.lutIDX_lutSamplerIDX_padding_padding.y
#define SHADOWCOUNT globals.lightcount_mode_shadowct_padding.z


#define MODEL_MATRIX GetTransform().Model
#define VIEW_MATRIX globals.viewMatrix
#define MODEL_VIEW_MATRIX mul(VIEW_MATRIX, MODEL_MATRIX)
#define PROJ_MATRIX globals.projMatrix
#define VIEW_PROJ_MATRIX mul(PROJ_MATRIX, VIEW_MATRIX)
#define MVP_MATRIX mul(PROJ_MATRIX, MODEL_VIEW_MATRIX)

#define WorldToClip(worldSPaceObject)  mul(PROJ_MATRIX, mul(VIEW_MATRIX, float4(worldSPaceObject.xyz, 1)))
#define WorldToView(worldSPaceObject)   mul(VIEW_MATRIX, float4(worldSPaceObject.xyz, 1))
#define ObjectToWorld(object)  mul(MODEL_MATRIX,object)
#define ObjectToView(object)  mul(MODEL_VIEW_MATRIX,object)
#define ObjectToClip(object)  mul(MVP_MATRIX, object)
#define ClipToNDC(object) object.xyz / object.w

//

//
//


// ShaderGlobals globals;
// uniformDescriptorSets
// storageDescriptorSets
// imageDescriptorSets
// samplerDescriptorSets
// [[vk::binding(0,0)]]
// [[vk::binding(0, 0)]]


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
cbuffer globals : register(b0, space1) { ShaderGlobals globals; }
[[vk::binding(1, 1)]]
Texture2DArray<float4> shadowmap[];
[[vk::binding(1, 1)]]
TextureCube shadowmapCube[];
[[vk::binding(2, 1)]]
SamplerState shadowmapSampler[];
[[vk::binding(3,1)]]
RWStructuredBuffer<LightData> lights;
[[vk::binding(4, 1)]]
RWStructuredBuffer<ObjectData> perObjectData;
[[vk::binding(5, 1)]]
RWStructuredBuffer<perShadowData> shadowMatrices;
[[vk::binding(5, 0)]]
RWStructuredBuffer<float4> positions;
[[vk::binding(6, 1)]]
RWStructuredBuffer<Transform> transforms;

#define  DIFFUSE_INDEX  perObjectData[InstanceIndex].props.textureIndexInfo.r
#define  SPECULAR_INDEX  perObjectData[InstanceIndex].props.textureIndexInfo.g
#define  NORMAL_INDEX perObjectData[InstanceIndex].props.textureIndexInfo.b

float3 GET_SPOT_LIGHT_DIR(LightData light)
{
    return light.lighttype_lightDir.yzw;
}

int getLightType(LightData light)
{
    return light.lighttype_lightDir.x;
}
float4x4 GetWorldToClipForLight(LightData light, uint offset)
{
    uint index = light.matrixIDX_matrixCt_padding.r + offset;
    return  mul(shadowMatrices[index].projMatrix,
                                 shadowMatrices[index].viewMatrix);
}
int getShadowMatrixIndex(LightData light)
{
    return light.matrixIDX_matrixCt_padding.r;
}


int getShadowMatrixCount(LightData light)
{
    return light.matrixIDX_matrixCt_padding.g;
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
    float PI = 3.14159265359;

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



float VectorToDepthValue(float3 Vec)
{
    float3 AbsVec = abs(Vec);
    float LocalZcomp = max(AbsVec.x, max(AbsVec.y, AbsVec.z));

    const float FAR = 10.0;
    const float NEAR = 0.001;
    float NormZComp = (FAR + NEAR) / (FAR - NEAR) - (2.0 * FAR * NEAR) / (FAR - NEAR) / LocalZcomp;
    return (NormZComp + 1.0) * 0.5;
}

int findCascadeLevel(int lightIndex, float3 worldPixel)
{
    float4 fragPosViewSpace = mul(mul(globals.projMatrix, globals.viewMatrix), float4(worldPixel, 1.0));
    float depthValue = fragPosViewSpace.z;
    int cascadeLevel = 0;
    for (int i = 0; i < 6; i++) //6 == cascade count
    {
        if (depthValue < -shadowMatrices[lightIndex + i].depth)
        {
            cascadeLevel = i;
            break;
        }
    }

    return cascadeLevel;
}
static const int POISSON_SAMPLECOUNT = 12;
float SampleSoftShadow(Texture2DArray<float4> Texture, float depth, float3 shadowUV)
{
    float2 vTexelSize = float2(1.0 / SHADOW_MAP_SIZE, 1.0 / SHADOW_MAP_SIZE);
    //Poisson lookup
    //Version from https://web.engr.oregonstate.edu/~mjb/cs519/Projects/Papers/ShaderTricks.pdf -- placheolder
   
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
    cSampleAccum += Texture.Sample(shadowmapSampler[0], shadowUV.xyz) >depth;
    // Take 12 samples in disc
    for (int nTapIndex = 0; nTapIndex < POISSON_SAMPLECOUNT; nTapIndex++)
    {
        float2 vTapCoord = vTexelSize * vTaps[nTapIndex] * 1.5;

        // Accumulate samples
        cSampleAccum += (Texture.Sample(shadowmapSampler[0],
                                                 shadowUV.xyz + float3(vTapCoord.x, vTapCoord.y, 0)).r >
           depth);
    }
    return cSampleAccum;
}

float3 getShadow(int index, float3 fragPos)
{
    LightData light = lights[index];
    if (getLightType(light) == LIGHT_POINT)
    {
        //Not obvious how this code works -- depends on near/far plane staying the same (see vectortodepthvalue)
        float3 fragToLight = fragPos - light.position_range.xyz;
        float distLightSpace = VectorToDepthValue(light.position_range.xyz - fragPos);
        float3 shadow = shadowmapCube[index].SampleLevel(cubeSamplers[0], fragToLight, 0.0).r;
        return distLightSpace < (shadow.r); // float3(proj.z, shadow.r, 1.0);
    }
    if (getLightType(light) == LIGHT_SPOT)
    {
        int ARRAY_INDEX = 0;
        float4x4 lightMat = GetWorldToClipForLight(light, ARRAY_INDEX);
        float4 fragClipSpace = mul(lightMat, float4(fragPos, 1.0));
        float3 fragNDC = ClipToNDC(fragClipSpace);
        float3 shadowUV = fragNDC * 0.5 + 0.5;

        return shadowmap[index].Sample(shadowmapSampler[0], float3(shadowUV.xy, ARRAY_INDEX)).r < fragNDC.z;
    }
    if (getLightType(light) == LIGHT_DIR)
    {
        float _SHADOW_MAP_SIZE = (SHADOW_MAP_SIZE);
        float2 vTexelSize = float2(1.0 / SHADOW_MAP_SIZE, 1.0 / SHADOW_MAP_SIZE);
        int lightIndex = getShadowMatrixIndex(light);
        int cascadeLevel = findCascadeLevel(lightIndex, fragPos);
        float4 fragClipSpace = mul(GetWorldToClipForLight(light, lightIndex + cascadeLevel), float4(fragPos, 1.0));
        float3 fragNDC = ClipToNDC(fragClipSpace);
        float3 shadowUV = fragNDC * 0.5 + 0.5;
        shadowUV.z = cascadeLevel;

 
        float cSampleAccum = SampleSoftShadow(shadowmap[index],  fragNDC.z, shadowUV.xyz);
      
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

    if (getLightType(light) == LIGHT_DIR)
    {
        return GetDirLightDirection(light);
    }
    
    else //(getLightType(light) == LIGHT_POINT || getLightType(light) == LIGHT_SPOT)
    {
        return normalize(GetLightPosition(light) - (fragPos));
    }

}

float3 getLighting(float3 albedo, float3 inNormal, float3 FragPos, float3 F0, float3 roughness, float metallic)
{
    float PI = 3.14159265359;
    float3 viewDir = normalize(globals.eyePos - FragPos);
    float3 lightContribution = float3(0, 0, 0);
    for (int i = 0; i < LIGHTCOUNT; i++)
    {
        LightData light = lights[i];
        float3 lightColor = light.color_intensity.xyz * light.color_intensity.a;
        float3 radiance = lightColor;
        
        float3 lightPos = GetLightPosition(light);
        float3 lightDir = GetLightDir(light,  FragPos);
        float lightDistance = pow(length(lightPos - FragPos), 2);
        float3 halfwayDir = normalize(lightDir + viewDir);

        int lightType = getLightType(light);
        if (lightType == LIGHT_POINT || lightType == LIGHT_SPOT)
        {
            float attenuation = 1.0 / (lightDistance * lightDistance);
            radiance *= attenuation;
        }
        if (lightType == LIGHT_SPOT)
        {
            float3 spotlightDir = GET_SPOT_LIGHT_DIR(light);
            float theta = dot(normalize(lightDir), normalize(-spotlightDir));

            float cosCutoff = (1.0 - (GetDirLightMaxRadius(light)) / 180);
            float softFactor = 1.2; //TODO JS: paramaterize 
            float cosCutoffOuter = (1.0 - (GetDirLightMaxRadius(light) * softFactor) / 180);
            float epsilon = cosCutoff - cosCutoffOuter;

            radiance *= clamp((theta - cosCutoffOuter) / epsilon, 0.0, 1.0);
        }

        float3 Fresnel = fresnelSchlick(max(dot(halfwayDir, viewDir), 0.0), F0);
        float NDF = DistributionGGX(inNormal, halfwayDir, roughness);
        float G = GeometrySmith(inNormal, viewDir, lightDir, roughness);
        float3 numerator = NDF * G * Fresnel;
        float denominator = 4.0 * max(dot(inNormal, viewDir), 0.0) * max(dot(inNormal, lightDir), 0.0) + 0.0001;
        float3 specular = numerator / denominator;
        float3 kS = Fresnel;
        float3 kD = 3.0 - kS;

        kD *= 1.0 - metallic;
        float NdotL = max(dot(inNormal, lightDir), 0.0);
        float3 lightAdd = (kD * albedo / PI + specular) * radiance * NdotL;

        //Shadow:

        if (i < SHADOWCOUNT) //TODO JS: pass in max shadow casters?
        {
            float shadowMapValue = getShadow(i, FragPos);
            lightAdd *= shadowMapValue;
        }
        //

        lightContribution += lightAdd;
    }


    return lightContribution;
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