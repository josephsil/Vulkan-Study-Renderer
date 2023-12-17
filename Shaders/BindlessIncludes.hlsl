#define USE_RW

#define LIGHTCOUNT   globals.lightcount_mode_padding_padding.r
#define VERTEXOFFSET pc.indexInfo.g
#define TEXTURESAMPLERINDEX pc.indexInfo.b
#define NORMALSAMPLERINDEX TEXTURESAMPLERINDEX +2 //TODO JS: temporary!
#define OBJECTINDEX  pc.indexInfo.a
#define LIGHTINDEX   pc.indexInfo_2.a
#define SKYBOXLUTINDEX globals.lutIDX_lutSamplerIDX_padding_padding.x
#define SKYBOXLUTSAMPLERINDEX globals.lutIDX_lutSamplerIDX_padding_padding.y


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
    float4 lightcount_mode_padding_padding;
    float4 lutIDX_lutSamplerIDX_padding_padding;
};

struct pconstant
{
    float4 indexInfo;
    float roughness;
    float metallic;
    float _f1;
    float _f2;
    float4 indexInfo_2;
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
    float4 light_type_padding;
    float4x4 matrixViewProjeciton;
    // uint color;
};

cbuffer globals : register(b0) { ShaderGlobals globals; }
// ShaderGlobals globals;
// uniformDescriptorSets
// storageDescriptorSets
// imageDescriptorSets
// samplerDescriptorSets

[[vk::binding(0, 2)]]
Texture2D<float4> bindless_textures[];

[[vk::binding(0, 3)]]
SamplerState bindless_samplers[];

[[vk::binding(2, 2)]]
Texture2D<float4> shadowmap[];

[[vk::binding(2, 3)]]
SamplerState shadowmapSampler[];

[[vk::binding(0,1)]]
#ifdef USE_RW
RWStructuredBuffer<MyVertexStructure> BufferTable;
#else
ByteAddressBuffer BufferTable;
#endif
[[vk::binding(1,1)]]
RWStructuredBuffer<MyLightStructure> lights;
[[vk::binding(2, 1)]]
RWStructuredBuffer<UBO> uboarr;


[[vk::binding(1,2)]]
TextureCube cubes[];

[[vk::binding(1,3)]]
SamplerState cubeSamplers[];

[[vk::push_constant]]
pconstant pc;



#define  DIFFUSE_INDEX  (TEXTURESAMPLERINDEX * 3) + 0
#define  SPECULAR_INDEX  (TEXTURESAMPLERINDEX * 3) + 1
#define  NORMAL_INDEX  (TEXTURESAMPLERINDEX * 3) + 2





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

float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}


float3 getLighting(float4x4 model, float3 albedo, float3 inNormal, float3 FragPos, float3 F0, float3 roughness, float metallic)
{
    float PI = 3.14159265359;
    float3 viewDir = normalize(globals.viewPos - FragPos);
    float3 lightContribution = float3(0, 0, 0);
    for (int i = 0; i < LIGHTCOUNT; i++)
    {
        MyLightStructure light = lights[i];
        float3 lightPos = light.position_range.xyz;
        float3 lightColor = light.color_intensity.xyz * light.color_intensity.a;
        float3 lightDir;
        float3 lightToFrag = lightPos - (FragPos); // frag pos * 0 if it's a directional light

        if (light.light_type_padding[0] == 1)
        {
            lightDir = normalize(lightToFrag);
        }
        else
        {
            lightDir = lightPos;
        }
        
        float lightDistance = pow(length(lightPos - FragPos), 2);

        float3 halfwayDir = normalize(lightDir + viewDir);

        float3 radiance;
        if (light.light_type_padding[0] == 1)
        {
            float attenuation = 1.0 / (lightDistance * lightDistance);
            radiance = lightColor * attenuation;
        }
        else
        {
            radiance = lightColor;
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

        if (i == 0 || i == 1) //TODO JS: pass in max shadow casters?
        {
         
            float4x4 lightMat = light.matrixViewProjeciton;
            float4 fragPosLightSpace = mul( lightMat, float4(FragPos, 1.0));
            float3 shadowProjection = (fragPosLightSpace.xyz / fragPosLightSpace.w);
            float3 shadowUV = shadowProjection   * 0.5 + 0.5;
            // shadowProjection.y *= -1;
            float shadowMapValue =  shadowmap[0].Sample(shadowmapSampler[0], shadowUV.xy).r; //TODO JS: dont sample on branch?
            float shadow = (shadowProjection.z + 0.002) < (shadowMapValue) ? 1.0 : 0.0;
            //TODO: vias by normal
            //TODO: pcf
            //TODO: cascade
            lightAdd *= shadow;
        }

        lightContribution +=  lightAdd;
    }


    return lightContribution;
}