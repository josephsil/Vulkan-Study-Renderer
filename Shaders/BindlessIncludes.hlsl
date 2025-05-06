#define USE_RW
#include "structs.hlsl"
#define LIGHT_DIR 0
#define LIGHT_POINT 1
#define LIGHT_SPOT 2
#define LIGHTCOUNT   globals.lightcount_mode_shadowct_padding.r
#define VERTEXOFFSET uboarr[InstanceIndex].indexInfo.g
#define TEXTURESAMPLERINDEX  uboarr[InstanceIndex].textureIndexInfo.g
#define NORMALSAMPLERINDEX  uboarr[InstanceIndex].textureIndexInfo.b //TODO JS: temporary!
#define OBJECTINDEX  uboarr[InstanceIndex].indexInfo.a
#define SKYBOXLUTINDEX globals.lutIDX_lutSamplerIDX_padding_padding.x
#define SKYBOXLUTSAMPLERINDEX globals.lutIDX_lutSamplerIDX_padding_padding.y
#define SHADOWCOUNT globals.lightcount_mode_shadowct_padding.z
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

cbuffer globals : register(b0, space1) { ShaderGlobals globals; }

[[vk::binding(1, 0)]]
Texture2D<float4> bindless_textures[];
[[vk::binding(2,0)]]
TextureCube cubes[];

[[vk::binding(3, 1)]]
Texture2DArray<float4> shadowmap[];
[[vk::binding(3, 1)]]
TextureCube shadowmapCube[];

[[vk::binding(4, 0)]]
SamplerState bindless_samplers[];
[[vk::binding(5,0)]]
SamplerState cubeSamplers[];
[[vk::binding(6, 1)]]
SamplerState shadowmapSampler[];





[[vk::binding(7,0)]]
#ifdef USE_RW
RWStructuredBuffer<MyVertexStructure> BufferTable;
#else
ByteAddressBuffer BufferTable;
#endif

[[vk::binding(8,1)]]
RWStructuredBuffer<MyLightStructure> lights;
[[vk::binding(9, 1)]]
RWStructuredBuffer<objectData> uboarr;

// #ifdef SHADOWPASS
[[vk::binding(10, 1)]]
RWStructuredBuffer<perShadowData> shadowMatrices;
// #endif 

// #ifdef SHADOWPASS
[[vk::binding(11, 0)]]
RWStructuredBuffer<float4> positions;
// #endif 

// #ifdef SHADOWPASS
// #endif 







#define  DIFFUSE_INDEX  uboarr[InstanceIndex].textureIndexInfo.r
#define  SPECULAR_INDEX  uboarr[InstanceIndex].textureIndexInfo.g
#define  NORMAL_INDEX uboarr[InstanceIndex].textureIndexInfo.b

float3 GET_SPOT_LIGHT_DIR(MyLightStructure light)
{
    return light.lighttype_lightDir.yzw;
}

int getLightType(MyLightStructure light)
{
    return light.lighttype_lightDir.x;
}

int getShadowMatrixIndex(MyLightStructure light)
{
    return light.matrixIDX_matrixCt_padding.r;
}
int getShadowMatrixCount(MyLightStructure light)
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
    uv *= float2(0.5,0.33);
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

// int uvIndexFromDir(float3 dir)
// {
//     
// }

float VectorToDepthValue(float3 Vec)
{
    float3 AbsVec = abs(Vec);
    float LocalZcomp = max(AbsVec.x, max(AbsVec.y, AbsVec.z));

    const float FAR = 10.0;
    const float NEAR = 0.001;
    float NormZComp = (FAR+NEAR) / (FAR-NEAR) - (2.0*FAR*NEAR)/(FAR-NEAR)/LocalZcomp;
    return (NormZComp + 1.0) * 0.5;
}

int findCascadeLevel(int lightIndex, float3 worldPixel)
{
    float4 fragPosViewSpace = mul( mul(globals.projection,  globals.view), float4(worldPixel, 1.0));
    float depthValue = fragPosViewSpace.z;
    int cascadeLevel = 0;
    for (int i = 0; i < 6; i++) //6 == cascade count
        {
        if (depthValue <   -shadowMatrices[lightIndex + i].depth)
        {
            cascadeLevel = i;
            break;
        }
        }

    return cascadeLevel;

    
}


float3 getShadow(int index, float3 fragPos)
{
    MyLightStructure light = lights[index];
    if (getLightType(light) == LIGHT_POINT)
    {
      
        float3 fragToLight =  fragPos - light.position_range.xyz ;
        float3 dir = normalize(fragToLight);
        dir = dir.xyz;
        dir *= float3(1,1,1);

        float distLightSpace = VectorToDepthValue( light.position_range.xyz - fragPos);
        float dist =  distance(light.position_range.xyz, fragPos) ;
        float3 proj = dist / 10;
        float3 shadow = (shadowmapCube[index].SampleLevel(cubeSamplers[0], fragToLight, 0.0).r - 0.0) * 1;
        // shadow = pow(shadow, 700);
        
       // return proj.z;
        float output;
        return distLightSpace < (shadow.r ); // float3(proj.z, shadow.r, 1.0);
    }
    if (getLightType(light) == LIGHT_SPOT)
    {
        int ARRAY_INDEX = 0;
        float4x4 lightMat = mul(shadowMatrices[getShadowMatrixIndex(light) +ARRAY_INDEX].proj,  shadowMatrices[getShadowMatrixIndex(light) +ARRAY_INDEX].view);
        float4 fragPosLightSpace = mul( lightMat, float4(fragPos, 1.0));
        float3 shadowProjection = (fragPosLightSpace.xyz / fragPosLightSpace.w);
        float3 shadowUV = shadowProjection   * 0.5 + 0.5;
        shadowUV.z = ARRAY_INDEX;
        // shadowProjection.y *= -1;
        return shadowmap[index].Sample(shadowmapSampler[0], shadowUV.xyz).r <  shadowProjection.z; //TODO JS: dont sample on branch?
    }
    if (getLightType(light) == LIGHT_DIR)
    {

        float SHADOW_MAP_SIZE = 1024;
        float2 vTexelSize = float2( 1.0/SHADOW_MAP_SIZE , 1.0/SHADOW_MAP_SIZE  );
        int lightIndex = getShadowMatrixIndex(light);
        int cascadeLevel =  findCascadeLevel(lightIndex, fragPos);
            float4 fragPosShadowSpace = mul(mul(shadowMatrices[lightIndex + cascadeLevel].proj, shadowMatrices[lightIndex + cascadeLevel].view),  float4(fragPos, 1.0));
            float3 shadowProjection = (fragPosShadowSpace.xyz / fragPosShadowSpace.w);
            float3 shadowUV = shadowProjection   * 0.5 + 0.5;
            shadowUV.z = cascadeLevel;

        //Poisson lookup
        //Version from https://web.engr.oregonstate.edu/~mjb/cs519/Projects/Papers/ShaderTricks.pdf -- placheolder

        const int SAMPLECOUNT = 12;
        float2 vTaps[SAMPLECOUNT] = {float2(-0.326212,-0.40581),float2(-0.840144,-0.07358),
        float2(-0.695914,0.457137),float2(-0.203345,0.620716),
        float2(0.96234,-0.194983),float2(0.473434,-0.480026),
        float2(0.519456,0.767022),float2(0.185461,-0.893124),
        float2(0.507431,0.064425),float2(0.89642,0.412458),
        float2(-0.32194,-0.932615),float2(-0.791559,-0.59771)};

        float cSampleAccum = 0;
        // Take a sample at the disc’s center
        cSampleAccum += shadowmap[index].Sample( shadowmapSampler[0], shadowUV.xyz)  > shadowProjection.z;
        // Take 12 samples in disc
        for ( int nTapIndex = 0; nTapIndex < SAMPLECOUNT; nTapIndex++ )
        {
            float2 vTapCoord = vTexelSize * vTaps[nTapIndex] * 1.5;
            
            // Accumulate samples
            cSampleAccum += (shadowmap[index].Sample( shadowmapSampler[0],  shadowUV.xyz + float3(vTapCoord.x, vTapCoord.y, 0)).r > shadowProjection.z);
        }
        return saturate((cSampleAccum) / (SAMPLECOUNT + 1));
        }
        return -1;
}

float3 getLighting(float3 albedo, float3 inNormal, float3 FragPos, float3 F0, float3 roughness, float metallic)
{
    float PI = 3.14159265359;
    float3 viewDir = normalize(globals.viewPos - FragPos);
    float3 lightContribution = float3(0, 0, 0);
    // albedo = 1.0;
    // roughness = 1.0;
// metallic = 0.0;
    for (int i = 0; i < LIGHTCOUNT; i++)
    {
        MyLightStructure light = lights[i];
        float3 lightPos = light.position_range.xyz;
        float3 lightColor = light.color_intensity.xyz * light.color_intensity.a;
        float3 lightDir;
        float3 lightToFrag = lightPos - (FragPos); // frag pos * 0 if it's a directional light

        if (getLightType(light) == LIGHT_POINT || getLightType(light) == LIGHT_SPOT)
        {
            lightDir = normalize(lightToFrag);
        }
        else if (getLightType(light) == LIGHT_DIR)
        {
            lightDir = lightPos;
        }
        
        
        float lightDistance = pow(length(lightPos - FragPos), 2);

        float3 halfwayDir = normalize(lightDir + viewDir);

        float3 radiance = lightColor;
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

            float cosCutoff = (1.0 - (light.position_range.a) / 180);
            float softFactor = 1.2; //TODO JS: paramaterize 
            float cosCutoffOuter = (1.0 - (light.position_range.a * softFactor) / 180);
            float epsilon   = cosCutoff - cosCutoffOuter ;
            
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

            // if (i == 0)
            // {
            //     return  getShadow(i, FragPos);;
            // }
            // float shadow = shadowProjection.z < (shadowMapValue) ? 1.0 : 0.0;
            //TODO: vias by normal
            //TODO: cascade
            // return pow(shadowMapValue, 44);
           lightAdd *= shadowMapValue;
        }
        //

        lightContribution +=  lightAdd;
    }


    return lightContribution;
}